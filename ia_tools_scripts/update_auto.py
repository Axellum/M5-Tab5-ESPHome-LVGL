import re

with open("automations.yaml", "r", encoding="utf-8") as f:
    auto_content = f.read()

# We need to find the `count: 5` block under `calendar.get_events`
# and replace it with `count: 15` and add the extra fields.

old_block = """    - action: calendar.get_events
      data:
        start_date_time: "{{ now().strftime('%Y-%m-%d 00:00:00') }}"
        end_date_time: "{{ (now() + timedelta(days=5)).strftime('%Y-%m-%d 23:59:59') }}"
      target:
        entity_id: calendar.axxxums_gmail_com
      response_variable: agenda_events
    - action: weather.get_forecasts
      data:
        type: daily
      target:
        entity_id: weather.saint_vincent_de_tyrosse
      response_variable: daily_var
    - repeat:
        count: 5
        sequence:
          - action: esphome.tab5_ha_hmi_tab5_maj_previsions_jours
            data:
              jour: "{{ repeat.index - 1 }}"
              nom_jour: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {% if (repeat.index - 1) < fcasts|length %}
                  {% set dt = fcasts[repeat.index - 1].datetime | as_datetime %}
                  {% set days = ["Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"] %}
                  {{ days[dt.strftime('%w') | int] }}
                {% else %}
                  "N/A"
                {% endif %}
              condition: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ fcasts[repeat.index - 1].condition | default('unknown') if (repeat.index - 1) < fcasts|length else 'unknown' }}
              tmin: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ fcasts[repeat.index - 1].templow | float(0) if (repeat.index - 1) < fcasts|length else 0 }}
              tmax: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ fcasts[repeat.index - 1].temperature | float(0) if (repeat.index - 1) < fcasts|length else 0 }}
              est_repos: >
                {% set target_date = (now() + timedelta(days=repeat.index - 1)).strftime('%Y-%m-%d') %}
                {% set ns = namespace(travail=false) %}
                {% if agenda_events['calendar.axxxums_gmail_com'] is defined %}
                  {% for event in agenda_events['calendar.axxxums_gmail_com'].events %}
                    {% set ev_start = event.start | default(event.start_time, '') | string %}
                    {% set ev_end = event.end | default(event.end_time, '') | string %}
                    {% set ev_msg = event.summary | default(event.message, '') | string %}
                    {% if target_date in ev_start or target_date in ev_end %}
                      {% if 'Travail' in ev_msg %}
                        {% set ns.travail = true %}
                      {% endif %}
                    {% endif %}
                  {% endfor %}
                {% endif %}
                {{ not ns.travail }}
              est_dimanche: >
                {% set target_date = now() + timedelta(days=repeat.index - 1) %}
                {{ target_date.strftime('%w') == '0' }}"""

new_block = """    - action: calendar.get_events
      data:
        start_date_time: "{{ now().strftime('%Y-%m-%d 00:00:00') }}"
        end_date_time: "{{ (now() + timedelta(days=15)).strftime('%Y-%m-%d 23:59:59') }}"
      target:
        entity_id: calendar.axxxums_gmail_com
      response_variable: agenda_events
    - action: weather.get_forecasts
      data:
        type: daily
      target:
        entity_id: weather.saint_vincent_de_tyrosse
      response_variable: daily_var
    - repeat:
        count: 15
        sequence:
          - action: esphome.tab5_ha_hmi_tab5_maj_previsions_jours
            data:
              jour: "{{ repeat.index - 1 }}"
              nom_jour: >
                {% set target_dt = now() + timedelta(days=repeat.index - 1) %}
                {% set days = ["Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"] %}
                {% if repeat.index == 1 %}
                  Auj {{ target_dt.strftime('%d') }}
                {% else %}
                  {{ days[target_dt.strftime('%w') | int] }} {{ target_dt.strftime('%d') }}
                {% endif %}
              condition: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ fcasts[repeat.index - 1].condition | default('unknown') if (repeat.index - 1) < fcasts|length else 'unknown' }}
              tmin: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ fcasts[repeat.index - 1].templow | float(0) if (repeat.index - 1) < fcasts|length else 0 }}
              tmax: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ fcasts[repeat.index - 1].temperature | float(0) if (repeat.index - 1) < fcasts|length else 0 }}
              est_passe: >
                {% set fcasts = daily_var['weather.saint_vincent_de_tyrosse'].forecast %}
                {{ 'false' if (repeat.index - 1) < fcasts|length else 'true' }}
              est_repos: >
                {% set target_date = (now() + timedelta(days=repeat.index - 1)).strftime('%Y-%m-%d') %}
                {% set ns = namespace(travail=false) %}
                {% if agenda_events['calendar.axxxums_gmail_com'] is defined %}
                  {% for event in agenda_events['calendar.axxxums_gmail_com'].events %}
                    {% set ev_start = event.start | default(event.start_time, '') | string %}
                    {% set ev_end = event.end | default(event.end_time, '') | string %}
                    {% set ev_msg = event.summary | default(event.message, '') | string %}
                    {% if target_date in ev_start or target_date in ev_end %}
                      {% if 'Travail' in ev_msg %}
                        {% set ns.travail = true %}
                      {% endif %}
                    {% endif %}
                  {% endfor %}
                {% endif %}
                {{ not ns.travail }}
              est_dimanche: >
                {% set target_date = now() + timedelta(days=repeat.index - 1) %}
                {{ target_date.strftime('%w') == '0' }}
              heures_ouverture: >
                {% set target_date = (now() + timedelta(days=repeat.index - 1)).strftime('%Y-%m-%d') %}
                {% set ns = namespace(heures="") %}
                {% if agenda_events['calendar.axxxums_gmail_com'] is defined %}
                  {% for event in agenda_events['calendar.axxxums_gmail_com'].events %}
                    {% set ev_start = event.start | default(event.start_time, '') | string %}
                    {% set ev_end = event.end | default(event.end_time, '') | string %}
                    {% set ev_msg = event.summary | default(event.message, '') | string %}
                    {% if target_date in ev_start %}
                      {% if 'Travail' in ev_msg %}
                        {% set start_dt = ev_start | as_datetime %}
                        {% set end_dt = ev_end | as_datetime %}
                        {% set em = start_dt.strftime('%H:%M') if start_dt else '??' %}
                        {% set de = end_dt.strftime('%H:%M') if end_dt else '??' %}
                        {% set ns.heures = em ~ "-" ~ de %}
                      {% endif %}
                    {% endif %}
                  {% endfor %}
                {% endif %}
                {{ ns.heures }}"""

if old_block in auto_content:
    auto_content = auto_content.replace(old_block, new_block)
    with open("automations.yaml", "w", encoding="utf-8") as f:
        f.write(auto_content)
    print("Success")
else:
    print("Old block not found!")
