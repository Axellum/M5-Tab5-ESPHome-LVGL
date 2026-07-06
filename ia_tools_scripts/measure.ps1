Get-ChildItem "e:\AuxFilsDesIdees\00ProjetTab\Tab5" -Recurse -Filter "*.yaml" | Measure-Object -Property Length -Sum | Select Sum
(Get-Content -Path e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl.yaml).Count
