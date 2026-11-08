use super::{Activation, Object, object::ObjectWeak};
use crate::{
    display_object::{DisplayObject, TDisplayObject, TDisplayObjectContainer},
    string::{AvmAtom, AvmString, StringContext, WStr, WString},
};
use gc_arena::lock::Lock;
use gc_arena::{Collect, Gc, Mutation};
use ruffle_macros::istr;

#[derive(Clone, Debug, Collect)]
#[collect(no_drop)]
pub struct MovieClipPath<'gc> {
    /// The level that this path starts from
    level: i32,

    /// The elements of the path
    path_segments: Box<[AvmString<'gc>]>,

    /// The unparsed, original path
    /// This wastes some memory, but saves having to re-build it when coercing to a string
    full_path: AvmString<'gc>,
}

impl<'gc> MovieClipPath<'gc> {
    /// Clip references frequently snapshot the same path within a frame.
    /// Intern immutable text, while retaining each reference's original path
    /// and weak-object cache independently (including Flash rename quirks).
    fn new_interned(strings: &mut StringContext<'gc>, atom: AvmAtom<'gc>) -> Self {
        let full_path: AvmString<'gc> = atom.into();
        let mut parts = full_path.as_wstr().split(b'.');
        let level = parts
            .next()
            .and_then(|root| root.strip_prefix(WStr::from_units(b"_level")))
            .and_then(|s| s.parse::<i32>().ok())
            .unwrap_or(0);
        let path_segments = parts.map(|part| strings.intern_wstr(part).into()).collect();
        Self { level, path_segments, full_path }
    }

    /// Convert a path to a clip into a `MovieClipPath`
    fn new_from_path(mc: &Mutation<'gc>, path: WString) -> Self {
        let mut level = 0;

        // Break up the path
        let mut parts = path.as_wstr().split(b'.');

        // Parse out the level id, to support multi-file movies
        if let Some(level_id) = parts
            .next()
            .and_then(|root| root.strip_prefix(WStr::from_units(b"_level")))
            .and_then(|s| s.parse::<i32>().ok())
        {
            level = level_id;
        }

        Self {
            level,
            // Get the rest of the path
            path_segments: parts.map(|s| AvmString::new(mc, s)).collect(),
            full_path: AvmString::new(mc, path),
        }
    }
}

/// Validated, immutable text only. Never shares a reference's mutable weak-object
/// resolution state. Object addresses are lookup hints, not dereferenced handles:
/// even allocator address reuse must pass the complete current ancestry check.
#[cfg(feature = "puffle-path-snapshots")]
#[derive(Collect)]
#[collect(no_drop)]
pub(crate) struct ClipPathSnapshot<'gc> {
    names_leaf_first: Box<[Option<AvmString<'gc>>]>,
    root_depth: i32,
    path: Gc<'gc, MovieClipPath<'gc>>,
}

#[cfg(feature = "puffle-path-snapshots")]
impl<'gc> ClipPathSnapshot<'gc> {
    fn matches(&self, mut object: DisplayObject<'gc>) -> bool {
        for name in &self.names_leaf_first {
            let Some(parent) = object.avm1_parent() else { return false; };
            if object.name() != *name { return false; }
            object = parent;
        }
        object.avm1_parent().is_none() && object.depth() == self.root_depth
    }

    fn capture(mut object: DisplayObject<'gc>, path: Gc<'gc, MovieClipPath<'gc>>) -> Self {
        let mut names = Vec::new();
        while let Some(parent) = object.avm1_parent() {
            names.push(object.name());
            object = parent;
        }
        Self { names_leaf_first: names.into_boxed_slice(), root_depth: object.depth(), path }
    }
}

/// Represents a reference to a movie clip in AVM1
/// This consists of a string path which will be resolved to a target value when used
/// This also handles caching to maintain performance
#[derive(Copy, Clone, Debug, Collect)]
#[collect(no_drop)]
pub struct MovieClipReference<'gc>(Gc<'gc, MovieClipReferenceData<'gc>>);

#[derive(Clone, Debug, Collect)]
#[collect(no_drop)]
struct MovieClipReferenceData<'gc> {
    /// The path to the target clip
    path: Gc<'gc, MovieClipPath<'gc>>,

    /// A weak reference to the target stage object that `path` points to
    /// This is used for fast-path resolving when possible, as well as for re-generating `path` (in the case the target object is renamed)
    /// If this is `None` then we have previously missed the cache, due to the target object being removed and re-created, causing us to fall back to the slow path resolution
    cached_object: Lock<Option<ObjectWeak<'gc>>>,
}

impl<'gc> MovieClipReference<'gc> {
    pub fn try_from_stage_object(
        activation: &mut Activation<'_, 'gc>,
        object: Object<'gc>,
    ) -> Option<Self> {
        // We can't use as_display_object here as we explicitly don't want to convert `SuperObjects`
        let display_object = object.as_display_object_no_super()?;
        let display_object = if let DisplayObject::MovieClip(_) = display_object {
            display_object
        } else if activation.swf_version() <= 5 {
            let display_object = Self::process_swf5_references(activation, display_object)?;

            display_object
        } else {
            return None;
        };

        let cached = display_object.object1()?;
        #[cfg(feature = "puffle-path-snapshots")]
        let snapshot_path = activation.context.avm1.clip_path_snapshots
            .get(&(display_object.as_ptr() as usize))
            .filter(|snapshot| snapshot.matches(display_object))
            .map(|snapshot| snapshot.path);
        #[cfg(not(feature = "puffle-path-snapshots"))]
        let snapshot_path: Option<Gc<'gc, MovieClipPath<'gc>>> = None;
        let path = if let Some(path) = snapshot_path {
            path
        } else {
        let text = display_object.path();
        let atom = activation.context.strings.intern_wstr(text.as_wstr());
        let path = if let Some(path) = activation.context.avm1.movie_clip_paths.get(&atom) {
            *path
        } else {
            let value = MovieClipPath::new_interned(&mut activation.context.strings, atom);
            let path = Gc::new(activation.gc(), value);
            let cache = &mut activation.context.avm1.movie_clip_paths;
            if cache.len() >= 128 {
                if let Some(key) = cache.keys().next().copied() { cache.remove(&key); }
            }
            cache.insert(atom, path);
            path
        };
        #[cfg(feature = "puffle-path-snapshots")]
        {
            let snapshot = ClipPathSnapshot::capture(display_object, path);
            let cache = &mut activation.context.avm1.clip_path_snapshots;
            // Bounded per-player storage; holds strings, never display objects.
            if cache.len() >= 256 {
                if let Some(key) = cache.keys().next().copied() { cache.remove(&key); }
            }
            cache.insert(display_object.as_ptr() as usize, snapshot);
        }
        path
        };
        let mc_ref = MovieClipReferenceData {
            path,
            cached_object: Some(cached.as_weak()).into(),
        };
        Some(Self(Gc::new(activation.gc(), mc_ref)))
    }

    pub fn from_path(gc: &'gc Mutation<'gc>, path: &WStr) -> Self {
        let mc_ref = MovieClipReferenceData {
            path: Gc::new(gc, MovieClipPath::new_from_path(gc, path.to_owned())),
            cached_object: None.into(),
        };
        Self(Gc::new(gc, mc_ref))
    }

    /// Handle the logic of swfv5 DisplayObjects
    fn process_swf5_references(
        activation: &mut Activation<'_, 'gc>,
        mut display_object: DisplayObject<'gc>,
    ) -> Option<DisplayObject<'gc>> {
        // In swfv5 paths resolve to the first MovieClip parent if the target isn't a movieclip
        if activation.swf_version() <= 5 {
            while display_object.as_movie_clip().is_none() {
                #[allow(clippy::question_mark)]
                if let Some(p) = display_object.avm1_parent() {
                    display_object = p;
                } else {
                    // Somehow we have gotten an object that has no MovieClip up the chain
                    return None;
                }
            }
        }

        Some(display_object)
    }

    /// Resolve this reference to an object
    /// First tuple param indicates if this path came from the cache or not
    pub fn resolve_reference(
        self,
        activation: &mut Activation<'_, 'gc>,
    ) -> Option<(bool, Object<'gc>, DisplayObject<'gc>)> {
        // Check if we can re-use the cached `DisplayObject`, if we can then take this fast path
        if let Some(cache) = self
            .0
            .cached_object
            .get()
            .and_then(|c| c.upgrade(activation.gc()))
            && let Some(display_object) = cache.as_display_object_no_super()
        {
            // We have to fallback to manual path-walking if the object is removed
            if !display_object.avm1_removed() {
                let display_object = Self::process_swf5_references(activation, display_object)?;

                // Note that there is a bug here but this *is* how it works in Flash:
                // If we are using the cached DisplayObject, we return it's path, which can be changed by modifying `_name`
                // However, if we remove and re-create the clip, the stored path (the original path) will differ from the path of the cached object (the modified path)
                // Essentially, changes to `_name` are reverted after the clip is re-created

                return Some((true, cache, display_object));
            }
        }

        // We missed the cache, switch to always use the slow-path
        self.0.cached_object.take();

        // Either the GcWeak ref is gone, or the clip can't be used (not on stage etc.)
        // Here we manually parse the path, in order to find the target display object
        // This is different to how paths resolve in activation in two ways:
        // 1: We only handle slash-paths to display objects, other path type and paths to variables are *not* valid here
        // 2: We only interact with the DisplayObject tree, not scopes, if you shadow a display object in a path this needs to still resolve to the correct object, e.g:
        // var _level0 = 123;
        // trace(this.child);
        // Should correctly find the child. As `this` is Value::MovieClip("_level0.child"), we don't want to try and find `123.child`!

        // Get the level
        let mut start = Some(activation.get_or_create_level(self.0.path.level));

        // Keep traversing to find the target DisplayObject
        for part in self.0.path.path_segments.iter() {
            if let Some(s) = start
                && let Some(con) = s.as_container()
            {
                start = con.child_by_name(part, activation.is_case_sensitive());
            }
        }

        if let Some(start) = start {
            let display_object = Self::process_swf5_references(activation, start)?;

            Some((
                false,
                display_object.object1_or_bare(activation.gc()),
                display_object,
            ))
        } else {
            None
        }
    }

    /// Convert this reference to an `Object`
    pub fn coerce_to_object(self, activation: &mut Activation<'_, 'gc>) -> Option<Object<'gc>> {
        let (_, object, _) = self.resolve_reference(activation)?;
        Some(object)
    }

    /// Convert this reference to a `String`
    pub fn coerce_to_string(self, activation: &mut Activation<'_, 'gc>) -> AvmString<'gc> {
        match self.resolve_reference(activation) {
            // Couldn't find the reference
            None => istr!(""),
            // Found the reference, cached, we can't re-use `self.path` sadly, it would be quicker if we could
            // But if the clip has been re-named, since being created then `mc.path() != path`
            Some((true, _, dobj)) => AvmString::new(activation.gc(), dobj.path()),
            // Found the reference, un-cached, so our cached path must be correct
            Some((false, _, _)) => self.0.path.full_path,
        }
    }

    /// Get the path used for this reference
    pub fn path(self) -> AvmString<'gc> {
        self.0.path.full_path
    }
}
