# Sonde niveau 3 — session Code du 28 nov 2026

**Cible** : open-field 0.26 = base 44 (`893e59f`, HEAD `73292b5`), OS 1.7.3+0, open-xy `f10f6bd`.
**Livrables** : `v5/out/save-sonde.pack` (9 528 oct, SHA-256 `cc69400f8141dbe00f61421febcf341430fd634e9b3c9353add437565582b5ed`) + ce document.
**Ce qui a été fait** : lecture du code du base 44, build du pack, check. **Rien n'a tourné sur un Field.** Les verdicts A/B/C ci-dessous sont des *prédictions tirées du source du base 44* ; la sonde sert à les confirmer sur l'unité.

---

## 0. Résumé

Le code du base 44 répond déjà aux questions A, B et C, références à l'appui :

| Batterie | Prédiction (source base 44) | Ce que la sonde confirmera |
|---|---|---|
| A — sortir du dossier | **Fermé.** Nom avec `/` ou commençant par `.` → `BAD_NAME` (-5). Pas de variante racine de `op1f_path`. | A1 `NO_FILE`, A2–A6 `BAD_NAME`. **A8/A9 (antislash) = seule vraie inconnue.** |
| B — plafond d'écriture | **1 MiB exactement** (`LARGEST = 1 << 20`). Au-delà → `TOO_LARGE` (-2) avant tout accès disque. Pas d'écriture tronquée silencieuse possible (taille vérifiée après rename). | B1–B3 ok + relecture 100 % 0xAA ; B4–B7 `TOO_LARGE` puis `NO_FILE`. Durée de gel de la frame pour 1 MiB. |
| C — sous-dossiers | **Fermé.** `/` refusé dans les noms ; pas de `op1f_make_dir` dans aucun header ; une `place` de dossier avec `/` ou `..` est refusée par `pack_build.py` (essayé). | C1/C2 `BAD_NAME` ; C5 (antislash) inconnu ; C4 liste. |
| D — 0.25 → 0.26 | **Zéro changement** de l'API fichiers niveau 3. « Wagon » = instance d'effet dans un train fx, sans rapport avec les slots par cassette. | — |

**Recommandation provisoire : V2 (issue upstream précise), V3 en repli.** À confirmer ou corriger avec le `results.log` de l'unité. V1 n'est possible que si A8/A9 (antislash) passent *et* que 1 MiB suffit. V4 écarté.

---

## 1. Batterie D — reconnaissance passive

### 1.1 Prototypes niveau 3 (base 44)

`module/build_m3_card.h:324-342` (identiques dans `build_r1/r3/r5/r6e_card.h`) :

```cpp
int op1f_path  (const Op1fCard*, unsigned folder, const char* name, char* out, unsigned room);
int op1f_list  (const Op1fCard*, unsigned folder, char* names, unsigned room);
int op1f_read  (const Op1fCard*, unsigned folder, const char* name, void* buffer, unsigned room);
int op1f_write (const Op1fCard*, unsigned folder, const char* name, const void* data, unsigned bytes);
int op1f_remove(const Op1fCard*, unsigned folder, const char* name);   // R1, build_r6e_card.h:1342
```

Codes (`build_m3_card.h:128`) : `NO_FILE -1`, `TOO_LARGE -2`, `NOT_NOW -3`, `NO_FOLDER -4`, `BAD_NAME -5`, `FAILED -6`.

- **Pas** de `op1f.h`, **pas** de `docs/level-3.md` dans le repo : le niveau 3 est décrit dans `docs/base.md` (tableau l. 383) et dans le header.
- **Pas** de `op1f_make_dir` / `mkdir` (grep `module/` + `examples/` : 0 résultat).
- Implémentation réelle : `module/build_r2_folders.cpp`
  - `:219` `file_name()` : 1–39 caractères 0x20–0x7e, **pas de `/`**, pas de `.` en tête. **L'antislash `\` est accepté.**
  - `:138` `LARGEST = 1 << 20` ; `:472` `bytes > LARGEST` → `TOO_LARGE`, avant tout accès disque et avant lecture de `data`.
  - `:497` après rename, `file_size(path) != bytes` → `FAILED` : **pas de troncature silencieuse possible**.
  - `:442` `op1f_read` : `size > room` → `TOO_LARGE`, rien n'est lu ; pas de plafond propre en lecture hors `room`.
  - `:236-246` chemin toujours `user/pf/<place>/<nom>`, 64 caractères max.
  - `:205` `folder_text()` : place 1–15 car. `[A-Za-z0-9 _.-]`, sans `/`.

### 1.2 Diff 0.25 (`c53bb33`) → 0.26 (`893e59f`)

`git diff --stat c53bb33 893e59f -- docs module/*.h profiles/` : 10 fichiers, +301/−7.

- `module/build_r2_folders.cpp`, `module/build_m3_card.h` : **0 ligne changée** → ABI `op1f_read/write/list/path/remove` identique.
- `profiles/*`, `docs/firmware-interfaces.md` : **0 ligne** → adresses firmware identiques.
- `build_r6e_card.h` +117 : (1) options kind 3 `OP1F_OPTION_WORDS` (PR #56 : une fonction du pack fournit les mots d'une option au runtime, 32 max ; le fichier garde le mot, pas le numéro) ; (2) « hosted pages » : `op1f_rack_place`, `op1f_rack_kept`, `op1f_rack_page`, marque `op1f_hosted_1`, `OP1F_HOSTED_KEPT_MOST = 44`, `OP1F_KEEP_HOSTED = 16`.
- `op1f_wagon_art.h` +33 (PR #54 : image 80×64 d'un wagon, mot à +180 de l'ask d'effet).
- `docs/level-6.md`, `level-8.md` : doc des deux points ci-dessus ; `docs/base.md` : procédure de release du base uniquement.
- Aucune ask kind 9 `tapes`, aucun élargissement des chemins ni du plafond d'écriture.

### 1.3 « Wagon » et `op1f_rack_kept`

- Un **wagon** = une *instance d'un effet de pack exécutée par un hôte* (ex. un maillon d'un `fx train`) — `build_r6e_card.h`, partie « Base 44: hosted pages ». Chaque instance a un numéro 0–63, une place propre `OP1F_PLACE_HOSTED + n`, et au plus **44 octets de JSON** de réglages gardés.
- `int op1f_rack_kept(const Op1fCard* card, unsigned which, char* out, unsigned room)` (`build_r6e_card.h`, après `op1f_rack_place`) : l'*hôte*, depuis sa frame, récupère le texte gardé que l'effet a écrit pour l'instance `which` ; ≥2 (un objet), 0 si inchangé, <0 `NOT_HERE`/`TOO_LARGE`/`BAD_NAME`/`NOT_NOW`. L'hôte le sauve avec le wagon dans ses propres fichiers/trains et le rend via `op1f_rack_place`.
- **Verdict : sans rapport avec les slots synth/drum par cassette.** C'est un canal de 44 octets entre un train et ses effets, pas une persistance des sons firmware. **V4 écarté.**

### 1.4 Adresses firmware (OS 1.7.3+0) — `docs/firmware-interfaces.md`

`module/build_m4_base.cpp` n'existe pas (il y a `build_m4_loader.cpp`) ; les adresses de référence sont dans `profiles/op1f-1.7.3-interfaces.json`, documentées dans `docs/firmware-interfaces.md:31-195`. Inchangées en 0.26.

| Fonction | Adresse | | Fonction | Adresse |
|---|---|---|---|---|
| `file_size` | `0x8c051f9c` | | `json_ctor` | `0x8c085ba8` |
| `file_open` | `0x8fc8a9d8` | | `json_dtor` | `0x8c0844e0` |
| `file_read` | `0x8fc8a9a8` | | `json_read_file` | `0x8c0534d0` |
| `file_close` | `0x8fc8a994` | | `json_write_file` | `0x8c053168` |
| `file_seek` | `0x8c0a3630` | | `json_get` / `json_has` | `0x8c084f74` / `0x8c085068` |
| `make_folders` | `0x8c05206c` | | `json_lookup` / `json_assign` | `0x8c085e84` / `0x8c0845b8` |
| `file_remove` | `0x8fcbefb4` | | `alloc` / `free` (tas) | `0x8fcc7560` / `0x8fcc6c30` |
| `rename` | `0x8c051f88` | | `new` / `free` (C++) | `0x8d4e63bc` / `0x8d4e63cc` |
| `open`/`read`/`write`/`close` (classe) | `0x8fcba988` / `…954` / `…8c0` / `…8ac` | | `tape_root` / `tape_set` | `0x8d4f2b80` / `0x8d4f24dc` |
| `dir_open`/`dir_next`/`dir_close` | `0x8c051e28` / `0x8c0a4bd0` / `0x8c0a57d4` | | `tape_saver` / `notify` | `0x8c0c1dd0` / `0x8fca1ed2` |

⚠️ Le briefing v5 cite `0x8c0c1dc6` comme « writer tape » ; le profil a `tape_saver` à **`0x8c0c1dd0`**. À vérifier avant toute trampoline V3. `0x8c0c348c` (loader) n'est pas dans le profil.

### 1.5 Exemples qui écrivent sur disque

`dub.cpp:241/262`, `echo.cpp:422/442`, `sketch.cpp:199/237/253`, `list.cpp:92/154/163` : tous lisent/écrivent **un seul fichier plat** dans leur dossier unique, testent `answer == bytes`, traitent `NOT_NOW` comme « réessayer après le mode disque », le reste comme échec. **Aucun** ne gère de sous-dossier — l'API n'en a pas.

---

## 2. Le pack sonde

`v5/examples/save-sonde/` (à copier dans `examples/save-sonde/` d'un clone open-field pour rebuild) : `recipe.json`, `save-sonde.cpp` (~300 lignes), `save-sonde_card.s` (généré).

- Card **niveau 3**, un dossier montré `user/pf/save-sonde/`, **pas de fonction input**, **pas de profil propre = 0 place firmware**.
- Utilise `op1f_path/list/read/write/remove/log/ms`. `op1f_remove` vient de R1 (présent dans le base depuis R1) ; il marche pour une card niveau 3 à dossiers. Il ne sert qu'au nettoyage.
- Démarre à la frame 180 (~3 s après boot), uniquement mode disque fermé. **Un test par frame.** Après chaque test : une ligne au log (`save-sonde: …`) + réécriture de `results.log` (les résultats partiels survivent à un gel).
- **Une seule exécution** : si `results.log` finit par `END`, rien au boot suivant. Effacer `results.log` pour relancer.
- Lectures seulement en A : rien n'est écrit par les tests A ; ce qu'A pourrait atteindre est juste lu en mémoire, seule la taille est loggée.
- Buffer 1 MiB + 4 oct en BSS (zeroed 1 057 532 oct). B5–B7 passent `bytes` > buffer : le base refuse (`:472`) avant de lire `data` (voir risque résiduel §4).

### Tests, dans l'ordre

| ID | Opération | Attendu (source) |
|---|---|---|
| A0 | `op1f_path(folder 0, NULL)` | chemin `user/pf/save-sonde/` |
| A7 | `op1f_path(folder 0, "synth_slots.json")` | `user/pf/save-sonde/synth_slots.json` — pas de variante racine |
| A10 | `op1f_read` dossier 1 (pas à la card) | `NO_FOLDER` |
| A1 | `synth_slots.json` | `NO_FILE` |
| A2–A6 | `../…`, `../../user/…`, `/user/…`, `user/…`, `tape01/tape.json` | `BAD_NAME` |
| **A8** | `..\..\synth_slots.json` | **inconnu** — accepté par le base, dépend du système de fichiers de l'unité |
| **A9** | `..\..\..\user\synth_slots.json` | **inconnu** |
| B1–B3 | écriture 256 Ki / 512 Ki / 1 Mi de 0xAA, relecture, comptage des 0xAA | ok, compte = taille |
| B4–B7 | 1 Mi+1 / 1,5 / 2 / 2,125 Mi | `TOO_LARGE`, relecture `NO_FILE` |
| C1, C2 | `sub/t.txt`, `deep/nested/dir/t.txt` | `BAD_NAME` |
| **C5** | `sub\t.txt` (dans notre dossier) | **inconnu** — nom bizarre ou `FAILED` |
| C4 / C4b | liste dossier 0 / dossier 1 | noms / `NO_FOLDER` |
| Z1 | suppression b1–b7, C1, C2, C5 | 0 ou `NO_FILE`/`BAD_NAME` |
| Z2 | liste à nouveau | `results.log` seul |

Dans `results.log`, chaque test est suivi de `gap N ms` : le temps entre la frame du test et la suivante. Pour B3, c'est la durée du gel de l'affichage pour 1 MiB.

C3 (`op1f_make_dir`) n'existe pas : pas compilé. Équivalent build-time essayé : un 2e dossier de place `save-sonde/sub`, puis `../../tape01` → `pack_build.py card` refuse les deux (« is not a folder's: one to fifteen characters… »).

---

## 3. Build — ce qui a été construit, comment

- **Toolchain** : open-xy `f10f6bd` n'a pas d'archive Blackfin pour linux-x86_64 (`sdk.py toolchain fetch` → « No reviewed toolchain download ») ; son build Docker épinglé échoue ici (`snapshot.debian.org` bloqué par le réseau de la session). **Contournement** : fork `deanm1278/blackfin-plus-gnu@e4408a15` construit avec les patches et options de `tools/gcc/build-linux.sh` (0001 + macos/0003), mais avec le compilateur du conteneur (Ubuntu gcc 13.3) au lieu de Debian bookworm. Outils non identiques octet à octet aux pins Linux de `profiles/toolchains.json`.
- **Validation de cette toolchain** : `python -I tools/build_all.py` → **« the same » pour les 10 exemples et les 2 loaders** (octet pour octet vs `packs/built.json`). Seul `base.pack` « DIFFERS » — effet connu de l'ordre de l'hôte (open-xy `docs/toolchain.md`). **Le `base.pack` 44 publié sur l'unité est celui à utiliser, pas celui de ce build.**
- Commandes de la sonde (l'étape 4 du briefing ; `table_build` inutile, pas de profil propre) :
  ```bash
  python -I tools/pack_build.py card --recipe examples/save-sonde/recipe.json --out examples/save-sonde/save-sonde_card.s
  python -I tools/pack_build.py build3 --recipe examples/save-sonde/recipe.json --profile profiles/op1f-1.7.3.json --out out/save-sonde.pack
  python -I tools/pack_build.py check --pack out/save-sonde.pack
  ```
- **Check** : format 3, card niveau 3 à +5556, 1 dossier `user/pf/save-sonde/` montré, **0 stock change**, ne demande que `op1f_level_3, op1f_list, op1f_log, op1f_ms, op1f_path, op1f_read, op1f_remove, op1f_write`. Body 5 648 oct, zeroed 1 057 532 oct. `publish_check --forms-only` : 0 refusé.
- « Works with base 44 » : la marque requise `op1f_level_3` est offerte par tout base ≥ M3 ; base 44 l'offre.

---

## 4. Installation sur le Field (étape utilisateur)

1. Vérifier que l'unité est en 0.26 (écran de démarrage : base 44).
2. Mode disque → copier `save-sonde-2026-11-28.pack` dans `user/pk/` (le renommer `save-sonde.pack` si besoin).
3. Sortir du mode disque, redémarrer, attendre ~5 s sur n'importe quel écran. Chaque test peut geler brièvement l'affichage ; B3 (1 MiB) est le plus long.
4. Mode disque à nouveau : le dossier `save-sonde` montre `results.log`. Le récupérer et le renvoyer. Le log de l'unité a aussi les lignes `save-sonde: …`.
5. Désinstaller : supprimer le `.pack` de `user/pk/` (et le dossier `save-sonde` si on veut).

**Place** : le pack prend ~1,07 Mo de place pack. OK sous le loader N2 (16 MiB). **Sous N1** (2 031 616 oct, dont 1 296 388 déjà pris par le base + les 4 features) il ne tiendrait pas : le loader l'écarterait avec une ligne au log, sans dommage.

**Risques résiduels** :
- B3 écrit 1 MiB en flash en un seul appel : gel du thread d'affichage, peut-être une seconde ou plus ; 1 MiB d'usure temporaire, tout est effacé en Z1.
- B5–B7 passent un pointeur sur un buffer de 1 MiB avec `bytes` jusqu'à 2,125 MiB. Le source du base 44 refuse avant de lire. Si le base publié faisait autrement, il lirait jusqu'à ~1,1 MiB au-delà du buffer (mémoire des packs, lisible) et l'écrirait dans un fichier de notre dossier.
- A8/A9/C5 avec `\` : si le système de fichiers traite `\` comme séparateur, A8/A9 liraient `user/synth_slots.json` **en lecture seule** ; C5 reste dans notre dossier.
- En cas de gel : Help au boot (désactive les packs), puis suppression du `.pack` par MTP.

---

## 5. Grille de décision (provisoire, avant les résultats de l'unité)

- **V1 — niveau 3 suffit** : **peu probable.** Il faudrait qu'A8 ou A9 passe (faille antislash : fragile, non documentée, et en *lecture* seulement ; toute écriture reste confinée à notre dossier par les règles de nom). Même alors, 1 MiB est en dessous d'un synth stéréo plein (2,125 MiB) et C est fermé : impossible de cloner `tape0N/audio/...`.
- **V2 — issue upstream précise** : **recommandé** si A8/A9 échouent comme prévu. Les trois manques sont mesurés et localisés : (1) accès en lecture à `user/*_slots.json` et à `tape*/` (nouveau type de dossier de pack « lecture seule hors `user/pf/` », ou helper dédié `op1f_sound_get/put`) ; (2) plafond d'écriture au-dessus de 1 MiB, ou écriture par morceaux (`op1f_write_at`/append), puisque `LARGEST` est une seule constante (`build_r2_folders.cpp:138`) ; (3) sous-dossiers : `place` avec `/`, ou `op1f_make_dir`. Réfs : `build_m3_card.h:32-72, 118-128, 324-342` ; `build_r2_folders.cpp:205, 219, 472, 497`.
- **V3 — modifs firmware pack-level** : repli si l'upstream ne bouge pas. Les adresses firmware `file_*`, `json_*`, `alloc/free`, `tape_*` du §1.4 sont stables de 0.25 à 0.26. Revérifier `0x8c0c1dc6` vs `tape_saver` à `0x8c0c1dd0`.
- **V4 — wagon** : **écarté** (§1.3).

**Verdict définitif en attente de `results.log`.** La session chat suivante tranche une fois les mesures rentrées.

---

## 6. Non fait / hors scope

- Rien n'a tourné sur une unité. Pas de PR. Sources, pack et doc rangés dans `v5/` de `save-mod` (branche `v5`) ; `examples/save-mod/` et `out/save mod.pack` (v4 publié) intacts.
- Pas d'issue upstream rédigée (session chat suivante).
- Pas de vérification de `master mod` v8 / `save mod` v4 sous base 44 (chantier séparé).
- `project_write` n'était pas disponible dans la session Code : livraison par ce dépôt.
