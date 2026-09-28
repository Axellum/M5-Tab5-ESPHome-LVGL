#!/usr/bin/env bash
# [AI-CONTEXT]
# @file tools/ci/pip_reessai.sh
# @role `pip install` avec réessais, pour les workflows de la CI (28/09/2026).
#       PyPI répond parfois « Could not find a version that satisfies the requirement
#       esphome==2026.9.0 (from versions: none) » : un ou deux jobs sur six rataient
#       l'installation d'ESPHome (rendus, installation dans un HA neuf) alors que les
#       autres, au même moment, la réussissaient. Une croix rouge sans rapport avec le
#       code. Ici : 4 essais, 20 s puis 40 s puis 60 s d'attente, et les réessais de
#       pip lui-même sur les erreurs de connexion.
# @usage  bash tools/ci/pip_reessai.sh "esphome==$v" pillow
#         (mêmes arguments que `pip install`)
# @contrainte publication.yml n'utilise PAS ce script : il peut reconstruire un
#       ancien tag, qui ne l'a pas. Sa boucle est écrite dans le workflow.
set -u

essais=4
for essai in $(seq 1 "$essais"); do
  if pip install --retries 5 --timeout 30 "$@"; then
    exit 0
  fi
  if [ "$essai" -lt "$essais" ]; then
    attente=$((essai * 20))
    echo "::warning::pip install a échoué (essai $essai/$essais), nouvel essai dans $attente s : $*"
    sleep "$attente"
  fi
done
echo "::error::pip install a échoué $essais fois : $*"
exit 1
