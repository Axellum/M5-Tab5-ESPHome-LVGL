import urllib.request
import re
import json

req = urllib.request.urlopen("https://raw.githubusercontent.com/Templarian/MaterialDesign-Webfont/master/css/materialdesignicons.css")
css = req.read().decode('utf-8')
items = re.findall(r'\.mdi-(.*?)::before\s*\{\s*content:\s*"\\(.*?)"', css)
d = {k: v for k, v in items}

print(json.dumps({k: d[k] for k in ('arrow-up', 'arrow-down', 'pause', 'lightbulb', 'lightbulb-on') if k in d}, indent=2))
