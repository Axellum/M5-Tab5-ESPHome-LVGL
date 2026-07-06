import urllib.request, json
req = urllib.request.urlopen('https://raw.githubusercontent.com/Templarian/MaterialDesign-SVG/master/meta.json')
meta = json.loads(req.read())
icons_to_find = ['window-shutter', 'window-shutter-open', 'monitor', 'television']
for i in meta:
    if i['name'] in icons_to_find:
        print(f"{i['name']}: {i['codepoint']}")
