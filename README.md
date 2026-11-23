# save mod.pack

Un pack open-field pour OP-1 Field (OS 1.7.3+0, base de niveau 7 ou plus) qui garde **avec chaque cassette** la vitesse bande et les réglages des pages Mixer 2 (EQ), 3 (FX) et 4 (compresseur master).

Il ne modifie **aucun endroit du firmware** : il passe par le niveau 7 du base (`docs/level-7.md` d'open-field), qui garde un membre JSON à lui dans le fichier de la cassette.

## Ce que ça fait

Le fichier de chaque cassette reçoit un membre `save_mod` :

```json
"save_mod": {"eq":[0,0,0],"fx":[0,0,0,0,0,0,0,0],"master":[0,0,0,0],"tape_speed":0,"version":1}
```

| Membre | Correspond à |
|---|---|
| `eq` | Mixer page 2 : l'EQ master, 3 valeurs |
| `fx` | Mixer page 3 : les 8 valeurs de l'effet master actif (le type d'effet n'est pas gardé en v1) |
| `master` | Mixer page 4 : compresseur master, L, R, drive, release |
| `tape_speed` | vitesse bande, demi-tons × 1024, de −24 à +12 demi-tons |
| `version` | forme du membre, 1 |

Les valeurs `eq`, `fx` et `master` sont les mêmes nombres 16 bits signés que ceux que l'unité écrit dans `op1.json`.

- **Cassette chargée** (changement de cassette, cassette nouvelle ou copiée) : elle prend ses propres valeurs si elle a un `save_mod`. Sinon (cassette d'avant, cassette nouvelle), elle prend les valeurs que l'unité avait au démarrage, celles d'`op1.json`, et les garde ensuite.
- **Un réglage change** sur l'une des trois pages, ou la vitesse bande change : la valeur de la cassette suit. Elle est écrite dans le fichier quand l'unité écrit la cassette (c'est l'unité qui décide quand).
- **Un fichier édité à la main** avec une vitesse hors plage : elle est ramenée dans `[-24, +12]` demi-tons. Un membre qui n'a pas la forme de la version 1 est ignoré, et la cassette prend les valeurs du démarrage.

## Rétro-compatibilité

Le base n'écrit que le membre `save_mod` et ne touche jamais à un membre de l'unité. Sans le pack, ou sans open-field, le firmware lit le fichier comme d'habitude et ignore `save_mod`. Après le retrait du pack, le base continue de conserver le membre avec la cassette.

## Installation

Pré-requis : OS 1.7.3+0, open-field installé, `base.pack` dans `user/pk/`.

1. Copier `save mod.pack` dans `user/pk/`.
2. Éjecter et rebooter. Au démarrage, le log dit `keeps: 'save mod' keeps 1 value(s) in tapes` puis `save_mod: the values of the start kept`.

Retrait : supprimer le fichier et rebooter.

## Compatibilité

Aucun emplacement du firmware n'est modifié, donc pas de collision possible avec un autre pack. Le nom du membre, `save_mod`, est propre au pack. Un autre pack qui écrit lui aussi dans les variables de l'EQ, de l'effet ou du compresseur master au chargement d'une cassette entrerait en concurrence avec celui-ci.

## Construire

Dans un clone d'open-field avec la toolchain installée (`docs/build.md`) :

```bash
cp -r <ce dépôt>/examples/save-mod examples/
python -I tools/pack_build.py card --recipe examples/save-mod/recipe.json --out examples/save-mod/save-mod_card.s
python -I tools/pack_build.py build3 --recipe examples/save-mod/recipe.json --profile profiles/op1f-1.7.3.json --out "out/save mod.pack"
```

## Statut

- **v1** : `eq`, `fx`, `master`, `tape_speed`. Testé sur ordinateur uniquement (lecture et écriture du JSON). **Pas encore buildé, pas encore essayé sur une unité.**
- À vérifier sur l'unité : que l'EQ change bien de son tout de suite quand le pack écrit ses valeurs, et que la vitesse bande se met à jour par l'appel au setter avec un pas de 0.
- **v2** : `fx_type`, après le reverse de la variable et de la fonction qui change l'effet master.

## Licence

MIT.
