# save mod.pack

Un pack open-field pour OP-1 Field (OS 1.7.3+0, base de niveau 7 ou plus) qui garde **avec chaque cassette** la vitesse bande et les réglages des pages Mixer 2 (EQ), 3 (FX) et 4 (compresseur master).

Il ne modifie **aucun endroit du firmware** : il passe par le niveau 7 du base (`docs/level-7.md` d'open-field), qui garde un membre JSON à lui dans le fichier de la cassette.

## Ce que ça fait

Le fichier de chaque cassette reçoit un membre `save_mod` :

```json
"save_mod": {"eq":[0,0,0],"fx":[0,0,0,0,0,0,0,0],"fx_type":"delay","master":[0,0,0,0],"tape_speed":0,"version":2}
```

| Membre | Correspond à |
|---|---|
| `eq` | Mixer page 2 : l'EQ master, 3 valeurs |
| `fx` | Mixer page 3 : les 8 valeurs de l'effet master |
| `fx_type` | Mixer page 3 : l'effet master actif, sous le nom qu'`op1.json` lui donne (`delay`, `grid`, `punch`, `phone`, `spring`, `mother`, `nitro`, `cwo`, `vocoder`, `terminal`, `fazer`) |
| `master` | Mixer page 4 : compresseur master, L, R, drive, release |
| `tape_speed` | vitesse bande, demi-tons × 1024, de −24 à +12 demi-tons |
| `version` | forme du membre, 2 (la version 1, sans `fx_type`, est toujours lue : l'effet actif reste alors en place) |

Les valeurs `eq`, `fx` et `master` sont les mêmes nombres 16 bits signés que ceux que l'unité écrit dans `op1.json`.

- **Cassette chargée** (changement de cassette, cassette nouvelle ou copiée) : elle prend ses propres valeurs si elle a un `save_mod`. Si son effet master diffère de l'effet actif, le pack le pose comme le fait le chargement d'`op1.json` par l'unité, puis remet les 8 valeurs de la cassette. Les 4 valeurs des boutons sont bornées à la plage de l'effet actif. Une cassette sans `fx_type` (valeurs de la version 1) garde l'effet actif et ses valeurs : ses propres valeurs FX appartiennent peut-être à un autre effet. Sinon (cassette d'avant, cassette nouvelle), elle prend les valeurs que l'unité avait au démarrage, celles d'`op1.json`, et les garde ensuite.
- **Un réglage change** sur l'une des trois pages, ou la vitesse bande change : la valeur de la cassette suit. Elle est écrite dans le fichier quand l'unité écrit la cassette (c'est l'unité qui décide quand).
- **Un fichier édité à la main** avec une vitesse hors plage : elle est ramenée dans `[-24, +12]` demi-tons. Un nom d'effet que l'unité ne connaît pas laisse l'effet actif en place. Un membre qui n'a pas la forme de la version 1 ou 2 est ignoré, et la cassette prend les valeurs du démarrage.

## Rétro-compatibilité

Le base n'écrit que le membre `save_mod` et ne touche jamais à un membre de l'unité. Sans le pack, ou sans open-field, le firmware lit le fichier comme d'habitude et ignore `save_mod`. Après le retrait du pack, le base continue de conserver le membre avec la cassette.

## Installation

Pré-requis : OS 1.7.3+0, open-field installé, `base.pack` dans `user/pk/`.

1. Copier `save mod.pack` dans `user/pk/`.
2. Éjecter et rebooter. Au démarrage, le log dit `keeps: 'save mod' keeps 1 value(s) in tapes` puis `save_mod: the values of the start kept`.

Retrait : supprimer le fichier et rebooter.

## Compatibilité

Aucun emplacement du firmware n'est modifié, donc pas de collision possible avec un autre pack. Quand le pack change l'effet master, le base retire un effet de pack posé sur le master, comme quand l'unité, le MIDI ou `fx train` change l'effet. Un effet de pack sur le master n'est pas gardé par cassette. Le nom du membre, `save_mod`, est propre au pack. Un autre pack qui écrit lui aussi dans les variables de l'EQ, de l'effet ou du compresseur master au chargement d'une cassette entrerait en concurrence avec celui-ci.

## Construire

Dans un clone d'open-field avec la toolchain installée (`docs/build.md`) :

```bash
cp -r <ce dépôt>/examples/save-mod examples/
python -I tools/pack_build.py build3 --recipe examples/save-mod/recipe.json --profile profiles/op1f-1.7.3.json --out "out/save mod.pack"
```

## Statut

- **Version 3 du pack** (forme du membre : version 2). Buildé (`out/save mod.pack`, 5 700 octets), vérifié par `pack_build.py check`, code JSON et bornage testés sur ordinateur. **Pas encore essayée sur une unité.**
- **Version 1** : essayée sur une unité, tout se garde par cassette. Un gel a été vu en changeant d'effet. Cause probable : la v1 ne gardait pas l'effet et pouvait appliquer à un effet les valeurs d'un autre.
- **Version 2** : essayée sur une unité, pas de gel, mais l'effet rechargé n'était pas le bon (CWO revenu en Phone), et le log se remplissait de `Master fx param out of range`. La v2 passait le numéro de l'effet à la fonction de choix de la liste, qui attend un autre numéro. Corrigé en version 3.
- Après avoir écrit l'EQ, le pack appelle la fonction du firmware qui transmet ses valeurs au son. Après la vitesse bande, il appelle le setter avec un pas de 0. L'effet master est posé par la fonction que le chargement d'`op1.json` utilise.
- Le fichier `out/save mod.pack` a été buildé avec une toolchain Blackfin compilée sous Linux depuis le fork épinglé par open-xy. Avec cette toolchain, le loader et les exemples d'open-field sortent identiques octet pour octet aux références ; seul `base.pack` diffère légèrement.

## Licence

MIT.
