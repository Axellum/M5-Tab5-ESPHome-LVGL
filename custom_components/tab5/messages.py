# -*- coding: utf-8 -*-
"""Textes des notifications de l'intégration « Tab5 », dans les sept langues de l'écran.

[AI-CONTEXT]
@role Une notification persistante n'a pas de traduction dans Home Assistant (au contraire
      des formulaires et des réparations, translations/*.json) : le texte est choisi ici
      d'après la langue du serveur (hass.config.language, « fr », « de-CH »…), par
      langue_de() ; une langue absente retombe sur l'anglais. Module pur, sans import de
      homeassistant ni import relatif (tests/test_integration_tab5.py).
@ai_instruction Une table TEXTES par langue, mêmes clés et mêmes champs {…} partout
      (tests/test_integration_tab5.py le vérifie) ; le français est la référence. Phrases
      courtes, accents et guillemets de chaque langue ; jamais de code ni d'état brut à
      l'utilisateur, sauf un chemin de fichier entre accents graves.
"""
from __future__ import annotations

# Mêmes langues que translations/*.json et que l'écran (Tab5/lang/) ; même liste et même
# règle qu'assistant.LANGUES / assistant.langue_de (module pur lui aussi, test d'égalité).
LANGUES = ("fr", "en", "de", "nl", "es", "it", "tr")


def langue_de(langue: str | None) -> str:
    """« fr », « fr-FR », « de_CH »… → le code d'une des LANGUES ; sinon « en »."""
    code = (langue or "").lower().replace("_", "-").split("-")[0]
    return code if code in LANGUES else "en"


TEXTES: dict[str, dict[str, str]] = {
    "fr": {
        "inst_titre": "Tab5 : fichiers Home Assistant {version}",
        "inst_maj": "Fichiers du Tab5 passés de la {avant} à la {version} : {ecrits} remplacé(s), "
                    "{retires} retiré(s), {identiques} inchangé(s).",
        "inst_neuf": "Fichiers du Tab5 installés en {version} : {ecrits} fichier(s) posé(s).",
        "inst_sauvegarde": "Les anciens sont gardés dans `{sauvegarde}`.",
        "inst_modifies": "Modifiés à la main depuis la dernière installation, remplacés quand même "
                         "(copie dans la sauvegarde) : {liste}.",
        "inst_differents": "Déjà là et différents (copiés à la main ?), remplacés quand même "
                           "(copie dans la sauvegarde) : {liste}.",
        "inst_packages_absents": "Home Assistant ne charge pas encore les packages : voir Paramètres → Réparations.",
        "inst_redemarrer": "Redémarrez Home Assistant pour finir : voir Paramètres → Réparations.",
        "inst_fw_auto": "La tablette se mettra à jour toute seule dès que son entité « Firmware » "
                        "proposera la {version}.",
        "inst_fw_manuel": "Mettez ensuite la tablette à jour : Paramètres → Mises à jour, entité « Firmware ».",
        "inst_tableau": "Tableau de bord « Tab5 » : si cette version ajoute des entités, refaites ses étapes 2 et 3.",
        "act_creer": "Valider crée l'automatisation « Tab5 — emplacements de l'écran » (pièces et maison) dans "
                     "`automations.yaml` (sauvegardé avant dans `tab5_sauvegardes/automatisations/`), "
                     "puis recharge les automatisations. Tout reste modifiable ensuite dans l'automatisation.",
        "act_mettre_a_jour": "Une automatisation du blueprint existe déjà : « {alias} ». Elle ne change que "
                             "si vous cochez la case ci-dessous : ses pièces sont alors remplacées par "
                             "celles-ci, ses entrées de la maison par celles remplies ici, ses autres "
                             "réglages restent (`automations.yaml` sauvegardé avant). "
                             "Sans la case, elle ne change pas.",
        "act_ailleurs": "Une automatisation du blueprint existe déjà, hors de `automations.yaml` : "
                        "l'assistant n'y touche pas.",
        "act_plusieurs": "Plusieurs automatisations du blueprint existent déjà : l'assistant ne choisit pas "
                         "laquelle changer : il n'y touche pas.",
        "act_fichier": "L'assistant n'écrira pas `automations.yaml` ({raison}). Valider met le YAML à "
                       "coller dans une notification.",
        "res_titre": "Tab5 : assistant de configuration",
        "res_cree": "Automatisation des emplacements créée",
        "res_mis_a_jour": "Pièces de l'automatisation des emplacements remplacées",
        "res_non_chargee": "Automatisation écrite dans `automations.yaml`, mais Home Assistant "
                           "ne l'a pas chargée : voir Paramètres → Système → Journaux",
        "res_sauvegarde": "L'ancien `automations.yaml` est gardé dans `{sauvegarde}`.",
        "res_suite": "La tablette reçoit ses pièces à sa prochaine connexion. Tout se change ensuite dans "
                     "l'automatisation (Paramètres → Automatisations et scènes).",
        "res_a_coller": "`automations.yaml` n'a pas été écrit ({raison}). Ajoutez cette automatisation à "
                        "votre fichier des automatisations, puis rechargez les automatisations :",
        "res_inchangee": "Automatisation inchangée",
        "res_listes": "Listes « Tab5 · … » réglées : {liste}.",
        "res_ratees": "Pas réglées (à faire dans Paramètres → Appareils et services → Entités) : {liste}.",
        "sep": " ; ",
        "fw_titre": "Tab5 : mise à jour de la tablette",
        "fw_texte": "Les fichiers Home Assistant sont en place : la tablette passe en {version}. "
                    "Elle redémarre toute seule dans quelques minutes.",
    },
    "en": {
        "inst_titre": "Tab5: Home Assistant files {version}",
        "inst_maj": "Tab5 files updated from {avant} to {version}: {ecrits} replaced, "
                    "{retires} removed, {identiques} unchanged.",
        "inst_neuf": "Tab5 files installed, version {version}: {ecrits} file(s) added.",
        "inst_sauvegarde": "The old ones are kept in `{sauvegarde}`.",
        "inst_modifies": "Edited by hand since the last installation, replaced anyway "
                         "(a copy is in the backup): {liste}.",
        "inst_differents": "Already there and different (copied by hand?), replaced anyway "
                           "(a copy is in the backup): {liste}.",
        "inst_packages_absents": "Home Assistant does not load the packages yet: see Settings → Repairs.",
        "inst_redemarrer": "Restart Home Assistant to finish: see Settings → Repairs.",
        "inst_fw_auto": "The tablet will update by itself as soon as its « Firmware » entity offers {version}.",
        "inst_fw_manuel": "Then update the tablet: Settings → Updates, « Firmware » entity.",
        "inst_tableau": "« Tab5 » dashboard: if this version adds entities, do its items 2 and 3 again.",
        "act_creer": "Submitting creates the automation « Tab5 — screen slots » (rooms and home) in "
                     "`automations.yaml` (saved first in `tab5_sauvegardes/automatisations/`), then reloads the "
                     "automations. Everything stays editable in the automation afterwards.",
        "act_mettre_a_jour": "An automation of the blueprint already exists: « {alias} ». It only changes if "
                             "you tick the box below: its rooms are then replaced by these ones, its home inputs by "
                             "the ones filled in here, its other settings stay (`automations.yaml` saved "
                             "first). Without the box, it does not change.",
        "act_ailleurs": "An automation of the blueprint already exists outside `automations.yaml`: the "
                        "assistant leaves it alone.",
        "act_plusieurs": "Several automations of the blueprint already exist: the assistant does not pick "
                         "which one to change: it leaves them alone.",
        "act_fichier": "The assistant will not write `automations.yaml` ({raison}). Submitting puts the YAML "
                       "to paste in a notification.",
        "res_titre": "Tab5: setup assistant",
        "res_cree": "Screen slots automation created",
        "res_mis_a_jour": "Rooms of the screen slots automation replaced",
        "res_non_chargee": "Automation written to `automations.yaml`, but Home Assistant did "
                           "not load it: see Settings → System → Logs",
        "res_sauvegarde": "The old `automations.yaml` is kept in `{sauvegarde}`.",
        "res_suite": "The tablet gets its rooms at its next connection. Change anything afterwards in the "
                     "automation (Settings → Automations & scenes).",
        "res_a_coller": "`automations.yaml` was not written ({raison}). Add this automation to your "
                        "automations file, then reload the automations:",
        "res_inchangee": "Automation unchanged",
        "res_listes": "« Tab5 · … » lists set: {liste}.",
        "res_ratees": "Not set (do it in Settings → Devices & services → Entities): {liste}.",
        "sep": "; ",
        "fw_titre": "Tab5: tablet update",
        "fw_texte": "The Home Assistant files are in place: the tablet is updating to {version}. "
                    "It restarts by itself in a few minutes.",
    },
    "de": {
        "inst_titre": "Tab5: Home-Assistant-Dateien {version}",
        "inst_maj": "Tab5-Dateien von {avant} auf {version} aktualisiert: {ecrits} ersetzt, "
                    "{retires} entfernt, {identiques} unverändert.",
        "inst_neuf": "Tab5-Dateien in Version {version} installiert: {ecrits} Datei(en) abgelegt.",
        "inst_sauvegarde": "Die alten werden in `{sauvegarde}` aufbewahrt.",
        "inst_modifies": "Seit der letzten Installation von Hand geändert, trotzdem ersetzt "
                         "(Kopie in der Sicherung): {liste}.",
        "inst_differents": "Bereits vorhanden und anders (von Hand kopiert?), trotzdem ersetzt "
                           "(Kopie in der Sicherung): {liste}.",
        "inst_packages_absents": "Home Assistant lädt die Packages noch nicht: siehe Einstellungen → Reparaturen.",
        "inst_redemarrer": "Starten Sie Home Assistant zum Abschluss neu: siehe Einstellungen → Reparaturen.",
        "inst_fw_auto": "Das Tablet aktualisiert sich von selbst, sobald seine Entität „Firmware“ "
                        "die {version} anbietet.",
        "inst_fw_manuel": "Aktualisieren Sie danach das Tablet: Einstellungen → Aktualisierungen, Entität „Firmware“.",
        "inst_tableau": "Dashboard „Tab5“: Wenn diese Version Entitäten hinzufügt, führen Sie seine Schritte 2 "
                        "und 3 erneut aus.",
        "act_creer": "Bestätigen erstellt die Automation „Tab5 — Bildschirmplätze“ (Räume und Zuhause) in "
                     "`automations.yaml` (vorher in `tab5_sauvegardes/automatisations/` gesichert) und lädt dann "
                     "die Automationen neu. Danach bleibt alles in der Automation änderbar.",
        "act_mettre_a_jour": "Eine Automation des Blueprints existiert bereits: „{alias}“. Sie ändert sich nur, "
                             "wenn Sie das Kästchen unten ankreuzen: Dann werden ihre Räume durch diese ersetzt, "
                             "ihre Zuhause-Eingaben durch die hier ausgefüllten, ihre übrigen Einstellungen "
                             "bleiben (`automations.yaml` vorher gesichert). Ohne das Kästchen ändert sie sich nicht.",
        "act_ailleurs": "Eine Automation des Blueprints existiert bereits außerhalb von `automations.yaml`: "
                        "Der Assistent rührt sie nicht an.",
        "act_plusieurs": "Mehrere Automationen des Blueprints existieren bereits: Der Assistent wählt keine aus "
                         "und rührt sie nicht an.",
        "act_fichier": "Der Assistent schreibt `automations.yaml` nicht ({raison}). Bestätigen legt das "
                       "einzufügende YAML in eine Benachrichtigung.",
        "res_titre": "Tab5: Einrichtungsassistent",
        "res_cree": "Automation der Bildschirmplätze erstellt",
        "res_mis_a_jour": "Räume der Automation der Bildschirmplätze ersetzt",
        "res_non_chargee": "Automation in `automations.yaml` geschrieben, aber Home Assistant hat sie nicht "
                           "geladen: siehe Einstellungen → System → Protokolle",
        "res_sauvegarde": "Die alte `automations.yaml` wird in `{sauvegarde}` aufbewahrt.",
        "res_suite": "Das Tablet erhält seine Räume bei der nächsten Verbindung. Danach lässt sich alles in der "
                     "Automation ändern (Einstellungen → Automationen & Szenen).",
        "res_a_coller": "`automations.yaml` wurde nicht geschrieben ({raison}). Fügen Sie diese Automation Ihrer "
                        "Automationsdatei hinzu und laden Sie dann die Automationen neu:",
        "res_inchangee": "Automation unverändert",
        "res_listes": "Listen „Tab5 · …“ eingestellt: {liste}.",
        "res_ratees": "Nicht eingestellt (in Einstellungen → Geräte & Dienste → Entitäten erledigen): {liste}.",
        "sep": "; ",
        "fw_titre": "Tab5: Aktualisierung des Tablets",
        "fw_texte": "Die Home-Assistant-Dateien sind installiert: Das Tablet wechselt auf {version}. "
                    "Es startet in einigen Minuten von selbst neu.",
    },
    "nl": {
        "inst_titre": "Tab5: Home Assistant-bestanden {version}",
        "inst_maj": "Tab5-bestanden bijgewerkt van {avant} naar {version}: {ecrits} vervangen, "
                    "{retires} verwijderd, {identiques} ongewijzigd.",
        "inst_neuf": "Tab5-bestanden geïnstalleerd, versie {version}: {ecrits} bestand(en) geplaatst.",
        "inst_sauvegarde": "De oude worden bewaard in `{sauvegarde}`.",
        "inst_modifies": "Met de hand gewijzigd sinds de laatste installatie, toch vervangen "
                         "(kopie in de back-up): {liste}.",
        "inst_differents": "Al aanwezig en anders (met de hand gekopieerd?), toch vervangen "
                           "(kopie in de back-up): {liste}.",
        "inst_packages_absents": "Home Assistant laadt de packages nog niet: zie Instellingen → Reparaties.",
        "inst_redemarrer": "Herstart Home Assistant om af te ronden: zie Instellingen → Reparaties.",
        "inst_fw_auto": "De tablet werkt zichzelf bij zodra zijn entiteit “Firmware” {version} aanbiedt.",
        "inst_fw_manuel": "Werk daarna de tablet bij: Instellingen → Updates, entiteit “Firmware”.",
        "inst_tableau": "Dashboard “Tab5”: als deze versie entiteiten toevoegt, doe dan stap 2 en 3 opnieuw.",
        "act_creer": "Bevestigen maakt de automatisering “Tab5 — schermplaatsen” (kamers en huis) in "
                     "`automations.yaml` (eerst bewaard in `tab5_sauvegardes/automatisations/`) en herlaadt daarna "
                     "de automatiseringen. Alles blijft daarna aanpasbaar in de automatisering.",
        "act_mettre_a_jour": "Er bestaat al een automatisering van de blueprint: “{alias}”. Ze verandert alleen "
                             "als je het vakje hieronder aanvinkt: dan worden haar kamers vervangen door deze, "
                             "haar huisinstellingen door die je hier invult, en blijven haar andere instellingen "
                             "(`automations.yaml` eerst bewaard). Zonder het vakje verandert ze niet.",
        "act_ailleurs": "Er bestaat al een automatisering van de blueprint buiten `automations.yaml`: "
                        "de assistent laat ze met rust.",
        "act_plusieurs": "Er bestaan al meerdere automatiseringen van de blueprint: de assistent kiest er geen "
                         "en laat ze met rust.",
        "act_fichier": "De assistent schrijft `automations.yaml` niet ({raison}). Bevestigen zet de YAML om te "
                       "plakken in een melding.",
        "res_titre": "Tab5: configuratie-assistent",
        "res_cree": "Automatisering van de schermplaatsen gemaakt",
        "res_mis_a_jour": "Kamers van de automatisering van de schermplaatsen vervangen",
        "res_non_chargee": "Automatisering geschreven in `automations.yaml`, maar Home Assistant heeft ze niet "
                           "geladen: zie Instellingen → Systeem → Logboeken",
        "res_sauvegarde": "De oude `automations.yaml` wordt bewaard in `{sauvegarde}`.",
        "res_suite": "De tablet krijgt zijn kamers bij de volgende verbinding. Alles is daarna te wijzigen in de "
                     "automatisering (Instellingen → Automatiseringen & scènes).",
        "res_a_coller": "`automations.yaml` is niet geschreven ({raison}). Voeg deze automatisering toe aan je "
                        "automatiseringsbestand en herlaad daarna de automatiseringen:",
        "res_inchangee": "Automatisering ongewijzigd",
        "res_listes": "Lijsten “Tab5 · …” ingesteld: {liste}.",
        "res_ratees": "Niet ingesteld (doe het in Instellingen → Apparaten & diensten → Entiteiten): {liste}.",
        "sep": "; ",
        "fw_titre": "Tab5: update van de tablet",
        "fw_texte": "De Home Assistant-bestanden staan op hun plaats: de tablet gaat naar {version}. "
                    "Hij herstart vanzelf over enkele minuten.",
    },
    "es": {
        "inst_titre": "Tab5: archivos de Home Assistant {version}",
        "inst_maj": "Archivos del Tab5 actualizados de {avant} a {version}: {ecrits} sustituido(s), "
                    "{retires} retirado(s), {identiques} sin cambios.",
        "inst_neuf": "Archivos del Tab5 instalados en {version}: {ecrits} archivo(s) colocado(s).",
        "inst_sauvegarde": "Los anteriores se guardan en `{sauvegarde}`.",
        "inst_modifies": "Modificados a mano desde la última instalación, sustituidos de todos modos "
                         "(copia en la copia de seguridad): {liste}.",
        "inst_differents": "Ya existentes y distintos (¿copiados a mano?), sustituidos de todos modos "
                           "(copia en la copia de seguridad): {liste}.",
        "inst_packages_absents": "Home Assistant aún no carga los paquetes: vea Ajustes → Reparaciones.",
        "inst_redemarrer": "Reinicie Home Assistant para terminar: vea Ajustes → Reparaciones.",
        "inst_fw_auto": "La tableta se actualizará sola en cuanto su entidad «Firmware» ofrezca la {version}.",
        "inst_fw_manuel": "Después, actualice la tableta: Ajustes → Actualizaciones, entidad «Firmware».",
        "inst_tableau": "Panel «Tab5»: si esta versión añade entidades, repita sus pasos 2 y 3.",
        "act_creer": "Confirmar crea la automatización «Tab5 — posiciones de la pantalla» (habitaciones y casa) en "
                     "`automations.yaml` (guardado antes en `tab5_sauvegardes/automatisations/`) y luego recarga "
                     "las automatizaciones. Después todo sigue siendo editable en la automatización.",
        "act_mettre_a_jour": "Ya existe una automatización del blueprint: «{alias}». Solo cambia si marca la "
                             "casilla de abajo: entonces sus habitaciones se sustituyen por estas, sus entradas de "
                             "la casa por las rellenadas aquí, y sus demás ajustes se mantienen (`automations.yaml` "
                             "guardado antes). Sin la casilla, no cambia.",
        "act_ailleurs": "Ya existe una automatización del blueprint fuera de `automations.yaml`: "
                        "el asistente no la toca.",
        "act_plusieurs": "Ya existen varias automatizaciones del blueprint: el asistente no elige cuál cambiar "
                         "y no las toca.",
        "act_fichier": "El asistente no escribirá `automations.yaml` ({raison}). Confirmar pone el YAML que hay "
                       "que pegar en una notificación.",
        "res_titre": "Tab5: asistente de configuración",
        "res_cree": "Automatización de las posiciones creada",
        "res_mis_a_jour": "Habitaciones de la automatización de las posiciones sustituidas",
        "res_non_chargee": "Automatización escrita en `automations.yaml`, pero Home Assistant no la ha cargado: "
                           "vea Ajustes → Sistema → Registros",
        "res_sauvegarde": "El `automations.yaml` anterior se guarda en `{sauvegarde}`.",
        "res_suite": "La tableta recibe sus habitaciones en su próxima conexión. Después todo se cambia en la "
                     "automatización (Ajustes → Automatizaciones y escenas).",
        "res_a_coller": "`automations.yaml` no se ha escrito ({raison}). Añada esta automatización a su archivo "
                        "de automatizaciones y luego recargue las automatizaciones:",
        "res_inchangee": "Automatización sin cambios",
        "res_listes": "Listas «Tab5 · …» ajustadas: {liste}.",
        "res_ratees": "No ajustadas (hágalo en Ajustes → Dispositivos y servicios → Entidades): {liste}.",
        "sep": "; ",
        "fw_titre": "Tab5: actualización de la tableta",
        "fw_texte": "Los archivos de Home Assistant están instalados: la tableta pasa a {version}. "
                    "Se reinicia sola en unos minutos.",
    },
    "it": {
        "inst_titre": "Tab5: file di Home Assistant {version}",
        "inst_maj": "File del Tab5 aggiornati dalla {avant} alla {version}: {ecrits} sostituiti, "
                    "{retires} rimossi, {identiques} invariati.",
        "inst_neuf": "File del Tab5 installati nella {version}: {ecrits} file posati.",
        "inst_sauvegarde": "I vecchi sono conservati in `{sauvegarde}`.",
        "inst_modifies": "Modificati a mano dall'ultima installazione, sostituiti comunque "
                         "(copia nel backup): {liste}.",
        "inst_differents": "Già presenti e diversi (copiati a mano?), sostituiti comunque "
                           "(copia nel backup): {liste}.",
        "inst_packages_absents": "Home Assistant non carica ancora i package: vedi Impostazioni → Riparazioni.",
        "inst_redemarrer": "Riavvia Home Assistant per finire: vedi Impostazioni → Riparazioni.",
        "inst_fw_auto": "Il tablet si aggiornerà da solo appena la sua entità «Firmware» proporrà la {version}.",
        "inst_fw_manuel": "Poi aggiorna il tablet: Impostazioni → Aggiornamenti, entità «Firmware».",
        "inst_tableau": "Dashboard «Tab5»: se questa versione aggiunge entità, ripeti i suoi passi 2 e 3.",
        "act_creer": "Confermare crea l'automazione «Tab5 — posizioni dello schermo» (stanze e casa) in "
                     "`automations.yaml` (salvato prima in `tab5_sauvegardes/automatisations/`), poi ricarica le "
                     "automazioni. Tutto resta poi modificabile nell'automazione.",
        "act_mettre_a_jour": "Esiste già un'automazione del blueprint: «{alias}». Cambia solo se spunti la "
                             "casella qui sotto: allora le sue stanze sono sostituite da queste, i suoi input della "
                             "casa da quelli compilati qui, le altre impostazioni restano (`automations.yaml` "
                             "salvato prima). Senza la casella, non cambia.",
        "act_ailleurs": "Esiste già un'automazione del blueprint fuori da `automations.yaml`: "
                        "l'assistente non la tocca.",
        "act_plusieurs": "Esistono già più automazioni del blueprint: l'assistente non sceglie quale cambiare "
                         "e non le tocca.",
        "act_fichier": "L'assistente non scriverà `automations.yaml` ({raison}). Confermare mette lo YAML da "
                       "incollare in una notifica.",
        "res_titre": "Tab5: assistente di configurazione",
        "res_cree": "Automazione delle posizioni creata",
        "res_mis_a_jour": "Stanze dell'automazione delle posizioni sostituite",
        "res_non_chargee": "Automazione scritta in `automations.yaml`, ma Home Assistant non l'ha caricata: "
                           "vedi Impostazioni → Sistema → Registri",
        "res_sauvegarde": "Il vecchio `automations.yaml` è conservato in `{sauvegarde}`.",
        "res_suite": "Il tablet riceve le sue stanze alla prossima connessione. Poi tutto si cambia "
                     "nell'automazione (Impostazioni → Automazioni e scene).",
        "res_a_coller": "`automations.yaml` non è stato scritto ({raison}). Aggiungi questa automazione al tuo "
                        "file delle automazioni, poi ricarica le automazioni:",
        "res_inchangee": "Automazione invariata",
        "res_listes": "Liste «Tab5 · …» impostate: {liste}.",
        "res_ratees": "Non impostate (fallo in Impostazioni → Dispositivi e servizi → Entità): {liste}.",
        "sep": "; ",
        "fw_titre": "Tab5: aggiornamento del tablet",
        "fw_texte": "I file di Home Assistant sono installati: il tablet passa alla {version}. "
                    "Si riavvia da solo tra qualche minuto.",
    },
    "tr": {
        "inst_titre": "Tab5: Home Assistant dosyaları {version}",
        "inst_maj": "Tab5 dosyaları {avant} sürümünden {version} sürümüne güncellendi: {ecrits} değiştirildi, "
                    "{retires} kaldırıldı, {identiques} değişmedi.",
        "inst_neuf": "Tab5 dosyaları {version} sürümüyle kuruldu: {ecrits} dosya yerleştirildi.",
        "inst_sauvegarde": "Eskileri `{sauvegarde}` içinde saklanıyor.",
        "inst_modifies": "Son kurulumdan beri elle değiştirilmiş, yine de değiştirildi "
                         "(kopyası yedekte): {liste}.",
        "inst_differents": "Zaten vardı ve farklıydı (elle mi kopyalandı?), yine de değiştirildi "
                           "(kopyası yedekte): {liste}.",
        "inst_packages_absents": "Home Assistant paketleri henüz yüklemiyor: Ayarlar → Onarımlar bölümüne bakın.",
        "inst_redemarrer": "Bitirmek için Home Assistant'ı yeniden başlatın: Ayarlar → Onarımlar bölümüne bakın.",
        "inst_fw_auto": "Tablet, “Firmware” varlığı {version} sürümünü sunduğu anda kendini güncelleyecek.",
        "inst_fw_manuel": "Ardından tableti güncelleyin: Ayarlar → Güncellemeler, “Firmware” varlığı.",
        "inst_tableau": "“Tab5” panosu: bu sürüm varlık ekliyorsa 2. ve 3. adımlarını yeniden yapın.",
        "act_creer": "Onaylamak, `automations.yaml` içinde “Tab5 — ekran yerleşimi” otomasyonunu (odalar ve ev) "
                     "oluşturur (önce `tab5_sauvegardes/automatisations/` içine yedeklenir), ardından otomasyonları "
                     "yeniden yükler. Sonrasında her şey otomasyonda düzenlenebilir kalır.",
        "act_mettre_a_jour": "Blueprint'in bir otomasyonu zaten var: “{alias}”. Yalnızca aşağıdaki kutuyu "
                             "işaretlerseniz değişir: o zaman odaları bunlarla, ev girdileri burada doldurulanlarla "
                             "değiştirilir, diğer ayarları kalır (`automations.yaml` önce yedeklenir). Kutu "
                             "işaretlenmezse değişmez.",
        "act_ailleurs": "Blueprint'in bir otomasyonu `automations.yaml` dışında zaten var: asistan ona dokunmaz.",
        "act_plusieurs": "Blueprint'in birden fazla otomasyonu zaten var: asistan hangisinin değişeceğini seçmez, "
                         "onlara dokunmaz.",
        "act_fichier": "Asistan `automations.yaml` dosyasını yazmayacak ({raison}). Onaylamak, yapıştırılacak "
                       "YAML'ı bir bildirime koyar.",
        "res_titre": "Tab5: kurulum asistanı",
        "res_cree": "Ekran yerleşimi otomasyonu oluşturuldu",
        "res_mis_a_jour": "Ekran yerleşimi otomasyonunun odaları değiştirildi",
        "res_non_chargee": "Otomasyon `automations.yaml` dosyasına yazıldı, ancak Home Assistant onu yüklemedi: "
                           "Ayarlar → Sistem → Günlükler bölümüne bakın",
        "res_sauvegarde": "Eski `automations.yaml` `{sauvegarde}` içinde saklanıyor.",
        "res_suite": "Tablet odalarını bir sonraki bağlantısında alır. Sonrasında her şey otomasyonda "
                     "değiştirilebilir (Ayarlar → Otomasyonlar ve sahneler).",
        "res_a_coller": "`automations.yaml` yazılmadı ({raison}). Bu otomasyonu otomasyon dosyanıza ekleyin, "
                        "ardından otomasyonları yeniden yükleyin:",
        "res_inchangee": "Otomasyon değişmedi",
        "res_listes": "“Tab5 · …” listeleri ayarlandı: {liste}.",
        "res_ratees": "Ayarlanmadı (Ayarlar → Cihazlar ve hizmetler → Varlıklar bölümünden yapın): {liste}.",
        "sep": "; ",
        "fw_titre": "Tab5: tablet güncellemesi",
        "fw_texte": "Home Assistant dosyaları yerinde: tablet {version} sürümüne geçiyor. "
                    "Birkaç dakika içinde kendiliğinden yeniden başlar.",
    },
}


def _t(langue: str | None) -> dict[str, str]:
    return TEXTES[langue_de(langue)]


def installation(langue: str | None, *, avant: str | None, version: str, ecrits: int,
                 retires: int, identiques: int, sauvegarde: str | None, modifies: list[str],
                 redemarrer: bool, packages_absents: bool, firmware: str,
                 differents: list[str] | None = None) -> tuple[str, str]:
    """(titre, message) après une installation. `firmware` : « auto » (lancée dès que
    proposée), « manuel » (option décochée) ou « non » (fichiers pas actifs : rien)."""
    t = _t(langue)
    champs = {"avant": avant, "version": version, "ecrits": ecrits, "retires": retires,
              "identiques": identiques, "sauvegarde": sauvegarde}
    lignes = [t["inst_maj" if avant else "inst_neuf"].format(**champs)]
    if sauvegarde:
        lignes.append(t["inst_sauvegarde"].format(**champs))
    if modifies:
        lignes.append(t["inst_modifies"].format(liste=", ".join(modifies)))
    if differents:
        lignes.append(t["inst_differents"].format(liste=", ".join(differents)))
    if packages_absents:
        lignes.append(t["inst_packages_absents"])
    elif redemarrer:
        lignes.append(t["inst_redemarrer"])
    if firmware == "auto":
        lignes.append(t["inst_fw_auto"].format(version=version))
    elif firmware == "manuel":
        lignes.append(t["inst_fw_manuel"])
    lignes.append(t["inst_tableau"])
    return t["inst_titre"].format(version=version), "\n\n".join(lignes)


def assistant_action(langue: str | None, action: str, alias: str = "", raison: str = "") -> str:
    """Ce que fera la validation du récapitulatif de l'assistant (assistant.Situation.action)."""
    return _t(langue)[f"act_{action}"].format(alias=alias, raison=raison)


def assistant_resultat(langue: str | None, resultat: str, *, entite: str | None = None,
                       sauvegarde: str | None = None, yaml_a_coller: str = "",
                       raison: str = "", listes: list[str] | None = None,
                       listes_ratees: list[str] | None = None) -> tuple[str, str]:
    """(titre, message) à la fin de l'assistant. `resultat` : « cree », « mis_a_jour »,
    « non_chargee » (écrite, mais HA ne l'a pas chargée), « rien » ou « a_coller ».
    `listes` / `listes_ratees` : les listes « Tab5 · … » réglées ou pas (« libellé → choix »)."""
    t = _t(langue)
    lignes = []
    if resultat in ("cree", "mis_a_jour", "non_chargee"):
        lignes.append(t[f"res_{resultat}"] + (f" (`{entite}`)." if entite else "."))
        if sauvegarde:
            lignes.append(t["res_sauvegarde"].format(sauvegarde=sauvegarde))
        lignes.append(t["res_suite"])
    elif resultat == "a_coller":
        lignes.append(t["res_a_coller"].format(raison=raison))
        lignes.append(f"```yaml\n{yaml_a_coller.rstrip()}\n```")
    else:
        lignes.append(t["res_inchangee"] + (f" ({raison})." if raison else "."))
    if listes:
        lignes.append(t["res_listes"].format(liste=t["sep"].join(listes)))
    if listes_ratees:
        lignes.append(t["res_ratees"].format(liste=t["sep"].join(listes_ratees)))
    return t["res_titre"], "\n\n".join(lignes)


def firmware_lance(langue: str | None, version: str) -> tuple[str, str]:
    """(titre, message) quand la mise à jour de la tablette est lancée."""
    t = _t(langue)
    return t["fw_titre"], t["fw_texte"].format(version=version)
