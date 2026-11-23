# Session — pack `save mod` (persistance par cassette) — 23 novembre 2026

**Session** : Claude Code (cloud), à partir de `briefing-save-mod-pack-2026-11-23.md`.
**Référentiel** : `os.ldr` stock `d134eb1ac7ad88e8370d988329cd5d7419f016005b1d269034f9cf7166fad0ec` (2 079 016 oct.), OS 1.7.3+0, open-field 0.12 (`LaP0573/open-field` au commit `f43697a`, base v30).
**Résultat** : pack **version 3** essayé sur l'unité. La vitesse bande, l'EQ, l'effet master avec ses valeurs et le compresseur reviennent avec chaque cassette.
**Dépôt** : https://github.com/Reverdy-Carrasco/save-mod (`main`, dernier commit `5f3262d`). Auteur des commits : `save mod <save-mod@noreply.invalid>`.

---

## 1. Changement de design par rapport au briefing

Le design du briefing (2 trampolines sur le loader et le writer de `tape.json`) a été **abandonné** pour quatre raisons :

- **`0x8c0c348c` est déjà pris par le base.** Le base y saute vers `op1f_keeps_tape_load` (groupe `base` du profil). Deux packs ne peuvent pas modifier le même endroit : `save mod.pack`, trié après `base.pack`, aurait été écarté au boot.
- **Le writer est déjà accroché par le base.** Le base remplace l'appel à `0x8c0c1db8` (`op1f_keeps_tape_save_stub`), juste avant la fenêtre `0x8c0c1dc6` visée.
- **Le niveau 7 du base fait exactement ce qu'il faut.** Une demande `keeps` sur un membre JSON propre au pack, dans `tapeNN.json`. Le base prévient le pack à chaque chargement (`OP1F_KEPT_LOADED` / `OP1F_KEPT_MADE`) et écrit le membre quand l'unité écrit la cassette. Modèle : `examples/dub/`.
- **Les brouillons ne suivaient pas le format open-field.** Le profil n'avait ni schéma ni tables, il n'y avait pas de `*_table.s`, et les trampolines n'avaient pas de `RETS` sauvé ni de room.

**Design retenu** :
- un pack de **niveau 7**, avec **0 modification du firmware** ;
- une demande `keeps` sur le membre `save_mod`, gardé dans les cassettes. L'outil refuse `save mod` avec une espace comme nom de demande, d'où `save_mod`.

---

## 2. Format du membre (forme 2)

```json
"save_mod": {"eq":[3 valeurs],"fx":[8],"fx_type":"cwo","master":[4],"tape_speed":n,"version":2}
```

- **`eq`, `fx`, `master`** : des i16 signés, c'est-à-dire la moitié haute des mots i32 en RAM, comme dans `op1.json`.
- **`tape_speed`** : en demi-tons × 1024, borné à `[-24×1024, +12×1024]`.
- **`fx_type`** : le nom de l'effet master. Il est facultatif : quand il est absent, l'effet actif reste en place.
- **`version`** : la forme 1 (sans `fx_type`) est toujours lue. Toute autre version est ignorée.
- **Ordre** : les membres sont écrits dans l'ordre alphabétique de leurs noms, comme le base les rend.

**Comportement** :
- **Cassette avec `save_mod`** : ses valeurs sont appliquées.
- **Cassette sans `save_mod`** : elle prend les valeurs relevées à la 1re frame (celles d'`op1.json`), puis les garde.
- **Changement d'un réglage** : le pack le pose avec `op1f_value_set`.
- **Réponse `OP1F_NOT_NOW`** : le pack réessaie à la frame suivante, sans rien écrire dans le log.

---

## 3. Points de rencontre avec le firmware (vérifiés au désassemblage)

| Adresse | Rôle | Usage dans le pack |
|---|---|---|
| `0x849a8998` | 3 mots de l'EQ master | lus et écrits |
| `0x8c0bacb4` | transmet l'EQ au son (sans argument) | **appelée après l'écriture de l'EQ**. Sans elle, l'EQ ne changeait qu'à l'écran |
| `0x11903be0` | 8 mots de l'effet master, lus directement par le code audio | lus et écrits |
| `0x11b01e8c` | 4 mots du compresseur master, lus directement par le code audio | lus et écrits |
| `0x84c62228` | vitesse bande, i16, Q.10 | lue et écrite |
| `0x8d50ff34` | setter de vitesse `(choix, pas)` | appelé avec `(0, 0)` : il réapplique la vitesse dans sa plage (descripteur `0x840af8fe`) et met à jour la vitesse de lecture |
| `0x11902e24` | type de l'effet master actif (0 à 10) | lu et écrit |
| `0x8490e288` | 11 paires {nom, type} | conversion nom ↔ type |
| `0x8f008e28` | `(1, type)` construit l'effet master du type donné | **voie utilisée par le chargeur d'`op1.json`**. Le base intercepte la construction (`op1f_fx_make`) |
| `0x840afe60` | plages des boutons par type, remplies au démarrage (48 oct. par type, 6 par bouton : borne basse i16, borne haute u16) | les 4 premières valeurs FX sont bornées à la plage de l'effet actif |

**Liste des types** : `0 delay`, `1 grid`, `2 punch`, `3 phone`, `4 spring`, `5 mother`, `6 nitro`, `7 cwo`, `8 vocoder`, `9 terminal`, `10 fazer`. La liste du briefing était fausse : `chorus` n'existe pas.

**Ce que le firmware fait lui-même** :
- **Sauvegarde dans `op1.json`** : le writer convertit `[0x11902e24]` en nom (`0x8fca79b8`) et l'écrit sous la clé `fx_type` (table de clés `0x8400f330` + 0x24).
- **Chargement depuis `op1.json`** :
  1. il remplit les 8 valeurs FX ;
  2. il convertit le nom en type, puis appelle `0x8f008e28(1, type)` ;
  3. il écrit le type dans `[0x11902e24]`.
- **Piège trouvé en v2** : le choix depuis la liste du mixer passe par le mot `+0x28` de la table du contrôleur. Ce mot vaut `0x8400f2cc` en stock (`0x8c0bb684`) ; le base le remplace par `op1f_fx_master_pick`. Il attend le **champ `id` de l'entrée de la liste `fx_types`**, que le firmware lit dans un document de l'unité, et **non le type**. Les effets de packs ont des `id` à partir de 15. Appelé avec le type 7 (cwo), il a posé l'entrée dont l'`id` vaut 7, soit Phone sur l'unité.
- **Comportement du base** : à chaque frame, il compare le type du master à l'effet de pack posé dessus. S'ils ne correspondent plus, il retire ce placement, comme quand l'unité, le MIDI ou `fx train` change l'effet.

**Autres constats** :
- Les appels au setter de vitesse depuis `0x8c0c311e` et `0x8c0c313c` viennent des gestionnaires d'encodeur de la page tape. Il n'y a pas de persistance cachée.
- Le contrôleur du mixer est un singleton à `0x849a89c4`, dont l'accesseur est `0x8c0ba620()`. Il n'est plus utilisé en v3.

---

## 4. Historique des versions et essais sur l'unité

| Version | Contenu | Résultat sur l'unité |
|---|---|---|
| 1 | eq, fx, master, tape_speed (sans effet) | tout se garde par cassette. **Gel** vu en changeant d'effet. Cause probable : valeurs FX d'un effet appliquées à un autre |
| 2 | + `fx_type`, choix par l'entrée du contrôleur | pas de gel, mais **mauvais effet** (CWO revenu en Phone). Log : 5 333 lignes `Master fx param out of range` (pos1/pos2) après `load_tape 3` |
| 3 | effet posé par `0x8f008e28(1, type)` puis `[0x11902e24]` ; 4 boutons bornés à leur plage ; forme 1 → effet et valeurs FX gardés | **tout fonctionne parfaitement**. Essayé avec `fx train` 16, `lfo` 11, `master mod` 1, `sidechain` 10, `garden` 2 |

Le pack v3 fait 5 700 octets, SHA-256 `2907d84c03090cec4fa5249dee943526246a8ff45a296acbd7cbdc96d12b78b2`. Il ne demande au base que `op1f_level_7`, `op1f_log`, `op1f_value_get` et `op1f_value_set`. Pas de `memcpy` ni de division.

---

## 5. Toolchain et vérifications

- **Toolchain** : open-xy ne fournit pas de toolchain Linux, et sa recette Docker est bloquée (`snapshot.debian.org` refusé par le réseau de la session). Je l'ai compilée directement : fork `deanm1278/blackfin-plus-gnu` `e4408a15…`, correctifs `0001` et `macos/0003` d'open-xy, GCC 12 d'Ubuntu.
- **Validation de la toolchain** avec `build_all.py` d'open-field : le loader et les 11 exemples sont **identiques octet pour octet** aux références. Seul `base.pack` diffère (290 973 contre 291 049 octets). Ce n'est pas gênant : l'unité garde le `base.pack` officiel.
- **Vérifications de chaque build** :
  - `pack_build.py check` ;
  - `publish_check.py --stock` sur l'image : rien du firmware dans le dépôt ;
  - tests sur ordinateur du JSON (aller-retour, formes 1 et 2, noms inconnus, échappements, versions refusées) et du bornage.
- **Pas d'octets du firmware dans le dépôt.** Les désassemblages et les records sont restés hors dépôt.

---

## 6. Contenu du dépôt `Reverdy-Carrasco/save-mod`

```
LICENSE                              MIT, « the save mod authors »
README.md                            en anglais
examples/save-mod/recipe.json        version 3, card niveau 7, ask keeps save_mod
examples/save-mod/save-mod.cpp       la logique (frame + changed)
examples/save-mod/save-mod_card.s    généré par pack_build.py card
out/save mod.pack                    le pack buildé
```

**Build** : depuis un clone d'open-field avec la toolchain, copier `examples/save-mod`, puis :

```
python -I tools/pack_build.py build3 --recipe examples/save-mod/recipe.json --profile profiles/op1f-1.7.3.json --out "out/save mod.pack"
```

---

## 7. Pistes ouvertes

- **Catalogue open-field** : une entrée `catalog/packs.json` via une PR sur `LaP0573/open-field`, à vérifier avec `tools/catalog_check.py`. Pas faite : à demander explicitement.
- **Effet de pack sur le master** : il n'est pas gardé par cassette. Le base le garde lui-même (niveau 8, `OP1F_KEEP_MASTER`). Une piste pour une version future.
- **`fx_active`** (effet master allumé ou éteint) : pas gardé par cassette.
- **Licence** : remplacer « the save mod authors » par ton nom, si tu le souhaites.
