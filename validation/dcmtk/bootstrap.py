"""Use exactly the dependency pins and warning policy used by PACS CI."""
from pathlib import Path
import json
import re
import subprocess

action = Path('/workspace/pacs_system/.github/actions/checkout-kcenon-deps/action.yml')
pins = dict(re.findall(r"([A-Z_]+)_SHA: '([a-f0-9]{40})'", action.read_text()))
if len(pins) != 7:
    raise SystemExit('Expected seven CI dependency pins; update validation bootstrap for the new action')
for name, sha in pins.items():
    name = name.lower()
    subprocess.run(['git', 'init', name], check=True, cwd='/workspace')
    root = Path('/workspace')/name
    subprocess.run(['git','fetch','--depth','1',f'https://github.com/kcenon/{name}.git',sha], cwd=root, check=True)
    subprocess.run(['git','checkout','--detach','FETCH_HEAD'], cwd=root, check=True)
    if name != 'database_system':
        for path in root.rglob('*'):
            if path.name == 'CMakeLists.txt' or path.suffix == '.cmake':
                text = re.sub(r'"-Werror[^\"]*"', '', path.read_text())
                path.write_text(re.sub(r'-Werror[^ )\"]*', '', text))
Path('/workspace/dependency-pins.json').write_text(json.dumps(pins, indent=2)+'\n')
