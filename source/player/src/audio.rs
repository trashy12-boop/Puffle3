//! Ruffle's software mixer, consumed on the same PPE thread as the player.
use ruffle_core::backend::audio::{AudioBackend,AudioMixer,DecodeError,RegisterError,SoundHandle,SoundInstanceHandle,SoundStreamInfo,SoundTransform,swf};
pub struct Audio {pub mixer:AudioMixer}
impl AudioBackend for Audio {
 ruffle_core::impl_audio_mixer_backend!(mixer);
 fn play(&mut self) {}
 fn pause(&mut self) {}
}
