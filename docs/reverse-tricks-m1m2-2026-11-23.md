# Reverse M1/M2 (`trick_1` / `trick_2`) — OS 1.7.3+0

**Firmware** : `os.ldr` stock `d134eb1a…fad0ec`.
**Méthode** : outils d'open-field sur les records du firmware (`ldr_dump_records.py`, `bfin_callers.py`, `bfin_sweep.py`, `ldrmem.py`), hors dépôt. Rien du firmware n'est recopié ici : seulement les adresses, les rôles et la forme des données dont le pack a besoin.
**Résultat** : trouvé et confirmé par trois fonctions qui s'accordent.

## Les deux blocs

| Bloc | Adresse | Taille |
|---|---|---|
| M1 (`trick_1`) | `0x849a8a10` | 40 octets |
| M2 (`trick_2`) | `0x849a8a38` (M1 + 0x28) | 40 octets |

Layout d'un bloc :

| Décalage | Taille | Contenu | Dans `op1.json` |
|---|---|---|---|
| `+0` | mot 32 bits | la page du mixer que la mémoire rappelle, 0 à 3 (0 niveaux, 1 EQ, 2 effet, 3 compresseur) | `subscreen` (entier) |
| `+4` | 1 octet | 1 quand la mémoire contient quelque chose (les 3 octets suivants servent de remplissage) | `has_data` (booléen) |
| `+8` | 8 mots 32 bits | les 8 valeurs. Comme pour les pages, seule la moitié haute compte | `params` (8 entiers 16 bits) |

## Comment c'est trouvé

1. **Les clés.** Les chaînes `subscreen`, `has_data` et `params` sont juste avant la table de clés du mixer (`0x8400f330`, qui commence par `master`). `trick_1` est à la table + `0x44` et `trick_2` à la table + `0x4c`.
2. **L'écriture dans `op1.json`.** Le writer du mixer, après `fx_params`, appelle deux fois une fonction d'écriture de mémoire (`0x8c0ba958`) : une fois avec `0x849a8a10` sous la clé `trick_1`, une fois avec `0x849a8a10 + 0x28` sous la clé `trick_2`. Cette fonction écrit :
   - `subscreen` à partir du mot à `+0` ;
   - `has_data` en booléen à partir de l'octet à `+4` ;
   - `params` à partir des moitiés hautes des 8 mots à `+8`.
3. **La lecture depuis `op1.json`.** Le chargeur du mixer appelle, pour chaque clé présente, une fonction de lecture de mémoire (`0x8c0ba838`) avec les mêmes deux adresses. Elle fait l'inverse : un entier dans le mot à `+0`, 0 ou 1 dans l'octet à `+4`, chaque valeur décalée dans la moitié haute des mots à `+8`.
4. **La prise d'une mémoire.** `0x8c0ba8b4` prend `(mémoire, page)`, refuse une page au-delà de 3, met l'octet à `+4` à 1 et la page à `+0`, puis copie les valeurs de la page à partir de `+8`. Les sources sont celles des quatre pages déjà connues :

   | Page | Source | Taille copiée |
   |---|---|---|
   | niveaux | `0x11b01eb0` | 32 octets |
   | EQ | `0x849a8998` | 12 octets |
   | effet | `0x11903be0` | 32 octets |
   | compresseur | `0x11b01e8c` | 16 octets |

5. **Le choix d'un effet master par la liste** (`0x8c0bb684`). Pour chacune des deux mémoires dont la page est 2 (l'effet), il met l'octet à `+4` à 0 : choisir un autre effet vide une mémoire de l'effet.

## Ce que le pack en fait

- Il lit les deux blocs à chaque frame et les garde, dans le membre `save_mod`, sous `m1` et `m2` (`has_data`, `params`, `subscreen`).
- Au chargement d'une cassette, il les réécrit comme le fait le chargeur de l'unité : les valeurs, puis la page, puis l'octet. Il le fait après avoir posé l'effet master, puisque ce choix peut vider une mémoire de la page 2.
- Les valeurs des mémoires ne sont pas bornées par le pack : la plage dépend de la page et, pour l'effet, de l'effet actif. Le rappel d'une mémoire par l'unité fait ce qu'il fait d'une valeur de son propre fichier.

Le rappel par un appui court sur M1/M2 relit le bloc en RAM. Je n'ai pas cherché la fonction de rappel : le pack n'en a pas besoin, puisque l'unité relit le bloc à l'appui.
