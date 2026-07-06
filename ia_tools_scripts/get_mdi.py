import urllib.request, json
url_ttf = 'https://github.com/Templarian/MaterialDesign-Webfont/raw/master/fonts/materialdesignicons-webfont.ttf'
try:
    print("Downloading TTF...")
    urllib.request.urlretrieve(url_ttf, 'materialdesignicons-webfont.ttf')
    print("Downloading Meta JSON...")
    url_meta = 'https://raw.githubusercontent.com/Templarian/MaterialDesign-SVG/master/meta.json'
    req = urllib.request.urlopen(url_meta)
    meta = json.loads(req.read())
    icons_to_find = ['weather-sunny', 'snowflake-alert', 'snowflake', 'weather-snowy', 'weather-snowy-heavy', 'weather-windy', 'weather-lightning', 'home-flood', 'alert', 'waves', 'weather-pouring']
    for i in meta:
        if i['name'] in icons_to_find:
            print(f"{i['name']}: {i['codepoint']}")
    print('Done!')
except Exception as e:
    print(f"Error: {e}")
