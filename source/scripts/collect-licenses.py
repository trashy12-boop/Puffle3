from pathlib import Path
root=Path(__file__).resolve().parent.parent
out=root/'build/THIRD_PARTY_LICENSES.txt';out.parent.mkdir(exist_ok=True)
with out.open('w') as stream:
 for f in sorted((root/'licenses').rglob('*')):
  if f.is_file():stream.write('\n\n===== '+str(f.relative_to(root))+' =====\n\n'+f.read_text(errors='replace'))
