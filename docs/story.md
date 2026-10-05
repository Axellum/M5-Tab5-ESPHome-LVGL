# The story

## English · [Français](#version-française)

---

Having heard a lot about AI — especially for coding — a few months ago I wanted to see for myself what it could actually do. I needed a project, and my old Nextion screen (mostly weather, still on ESPHome and Météo-France) was starting to feel dated. So I decided to replace it — this time with a much more ambitious home-automation setup, on a far more capable display, driven by AI from end to end.

One thing led to another: I added a voice assistant to the screen, then a local “engine” to handle voice home-automation on-device / on-LAN, and semi-local or cloud paths for open conversation. That engine is still a work in progress (of course 🙂) — I put a lot (too much) into it to experiment and better understand how LLMs work: RAG, scoring, multi-LLM routing, MCP, and more. I share it mostly for information: I use it day-to-day with this screen, but not yet for coding, nor for everything I tried to pack in — some of which works more or less well.

I started with Antigravity, then leaned on various models (DeepSeek, MiniMax, Z.ai). Next I tried Claude, which also did a lot of the work, then Cursor more recently. In short: this is my everyday screen project — I am more its architect than its creator — born from my first steps into the world of AI.

**So, why a screen, though?**

After five years with the Nextion, I wanted to give my mostly-weather screen a facelift, keeping at least the same core goals:

- Be a clock.
- Warn me at a glance if rain is coming within the hour — do I leave 15 minutes early so I don't show up soaked at the office? Do I grab an umbrella?
- Show the forecast for the next few days, so I have a conversation topic for the rare occasions I decide to be social.
- Do all that on reasonable power, always on (well, whenever I'm actually in front of it), with total freedom over layout, design and logic — not just the stock Home Assistant dashboard — with all the upsides that come with it... and the downsides.

Then home-automation ambitions crept in:

- A direct readout of the soil moisture in my plant pots / veggie patch.
- Turn the TV and PC on without lifting my butt off the chair.
- Control the three living-room spotlights and the bedroom light.

Then, little by little...

- Control my AC.
- Operate the roller shutter — again, without getting up.
- Add "Ok Nabu" wake-word support, so I don't even have to lean over to grab the screen anymore 🙂
- Polish the voice assistant: a conversation mode, a choice of LLM (local or not), and home commands that are as fast as possible and actually understand me.
- Display my work schedule so I can read my shift hours at a glance.
- Get a network remote for my TV — it can always come in handy. (Funny story: Claude Fable 5 completely blew me away on that one — it built the whole thing in two prompts flat. Naturally, I then let it loose redoing *every single popup* in the project… at the cost of roughly 50% of my 5-hour usage cap per popup, on the Pro plan. Worth it.)

All of that with these design goals in mind. My very first sessions with Gemini, rewriting the old Nextion code, were honestly humbling — it rethought how the data got sent and cut the codebase to a third of its size. So this time I wanted lightness and much better optimization than anything I'd hand-rolled myself:

- No images anywhere — as light and optimized for the tablet as it gets.
- Data pushes on the Home Assistant side as gentle as possible (my HA runs on a Freebox box, so I have to stay lean).
- A fast boot with no display lag — something that just *feels* smooth.

I also aimed for a modern-feeling interface (I'm in my fifties — don't expect miracles): no separate pages, just popups, everything reachable via a button, a long-press, or a swipe. Heavy color-coding gives at-a-glance readability from a few meters away, even though the screen is really meant to be read from under a meter for the fine print. And I tried to pack the maximum info/controls onto something that stays reasonably clean — yes, I know, "clean" is the part I'm worst at. End goal: a screen that looks decent to the eye, even though deep down I'm way more about function than form.

Companion backend (optional, work-in-progress): **[vromvrom-engine](https://github.com/Axellum/vromvrom-engine)** — multi-agent orchestrator used for voice routing and conversation.

→ The linked projects: [related projects](related_projects.md) · How AI was used: [note on AI](../README.md#note-on-ai)

---

## Version Française

---

Ayant beaucoup entendu parler de l’IA, et notamment en codage, il y a quelques mois de ça j’ai voulu voir par moi-même ce que cela donnait. Il me fallait un projet, et comme mon vieux écran Nextion (plutôt météo, toujours avec ESPHome et Météo-France) commençait à dater, j’ai opté pour le renouveler — mais cette fois avec un aspect domotique bien plus poussé, sur un écran bien plus qualitatif et puissant, le tout piloté par l’IA.

De fil en aiguille, j’ai complété l’écran avec un assistant vocal, puis par un « moteur » pour gérer en local la partie domotique vocale, et en semi-local ou cloud la partie conversations. Le moteur est un projet en cours (lui aussi 🙂) où j’ai posé beaucoup (trop) de choses pour expérimenter et mieux comprendre comment marchent les LLMs : RAG, notations, gestion multi-LLM, MCP, et j’en passe. Je le partage donc surtout dans un but informatif : je l’utilise de façon fonctionnelle pour l’écran, mais pas encore pour le codage ni pour tout ce que j’ai voulu y implémenter — qui fonctionne plus ou moins bien.

Dans mon périple, j’ai commencé avec Antigravity, puis je l’ai aidé par différents modèles (DeepSeek, MiniMax, Z.ai). Ensuite, j’ai testé Claude, qui a fait lui aussi beaucoup de travail, puis Cursor récemment. Bref, je vous partage le projet de mon écran, fait pour mon usage quotidien, dont je suis plus l’architecte que le créateur — issu de mes débuts d’aventure dans le monde de l’IA.

**Bref, pourquoi un écran ?**

Après 5 ans avec le Nextion, je souhaitais donner un coup de jeune à mon écran plutôt axé météo, en gardant a minima les mêmes objectifs :

- faire office d’horloge ;
- voir au premier coup d’œil si des averses sont prévues dans l’heure : je pars 15 min en avance pour ne pas arriver trempé au boulot ? Je prévois le parapluie ?
- avoir la prévision météo sur quelques jours, histoire d’avoir un sujet de conversation si je décide de me sociabiliser ;
- le tout pour une consommation raisonnable et toujours allumé (enfin, quand je suis devant), avec une liberté totale sur les positionnements, designs et logiques — pas juste l’affichage HA standard — avec les avantages... et les inconvénients que ça implique.

Avec un esprit domotique plus poussé : un retour direct de l’humidité de mes pots / de mon potager, allumer la TV et l’ordi sans bouger mes fesses de ma chaise, et gérer les trois spots du salon et la lumière de la chambre.

Puis, petit à petit :

- gérer ma clim ;
- avoir la main sur mon volet roulant, toujours sans me lever ;
- intégrer « Ok Nabu », plus besoin de me pencher pour attraper l’écran :) ;
- peaufiner l’intégration de l’assistant vocal : mode conversation, choix du LLM (local ou pas), domotique la plus rapide possible et qui me comprend ;
- afficher mon planning avec une lisibilité rapide de mes heures d’embauche ;
- avoir une télécommande réseau pour ma TV, ça peut toujours dépanner (pour la petite histoire, Claude Fable 5 m’a bluffé sur ce coup : il m’a fait ça en 2 prompts, du coup je l’ai laissé reprendre tous les popups, au prix de 50 % de ma limite des 5h par popup, sur le forfait Pro...).

Le tout avec, en termes de conception, les objectifs suivants. Vu que mes premières sessions avec Gemini sur le code du Nextion m’ont littéralement humilié — il a révolutionné l’envoi des données et divisé le code par trois — je voulais cette fois de la légèreté et de bien meilleures optimisations que ce que j’avais fait à la main :

- pas d’images, le plus léger et optimisé possible pour la tablette ;
- une gestion des envois de données côté HA robuste et la plus douce possible (mon Home Assistant tourne sur une Freebox, je reste léger) ;
- un démarrage rapide, pas de lenteur d’affichage, quelque chose de fluide, quoi.

J’ai aussi essayé d’avoir une interface moderne (j’ai la cinquantaine, ne m’en demandez pas trop) : pas de pages, mais des popups, tout accessible directement depuis l’écran d’accueil par bouton, toucher long ou swipe. Beaucoup de code couleur pour une lisibilité même à quelques mètres, tout en ayant un écran pensé pour être lu à moins d’un mètre si on veut voir toutes les données correctement. Et j’ai essayé de caser un maximum d’infos et de commandes sur une interface relativement épurée — oui, je sais, le plus dur pour moi. Objectif final : un écran à peu près correct visuellement, même si je reste plus axé pratique dans l’absolu.

Backend compagnon (optionnel, en cours) : **[vromvrom-engine](https://github.com/Axellum/vromvrom-engine)** — orchestrateur multi-agents utilisé pour le routage vocal et la conversation.

→ Les projets liés : [projets voisins](related_projects.md#version-française) · Comment l'IA a été utilisée : [note sur l'IA](../README.md#note-sur-lia)
