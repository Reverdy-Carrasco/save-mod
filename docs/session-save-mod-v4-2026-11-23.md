# Session — `save mod` 4 (membre forme 3) — 23 novembre 2026

**Point de départ :** v3 sur `main` (`19d4693`), essayée sur l'unité. Briefing `briefing-save-mod-v4-2026-11-23.md`.
**Résultat :** v4 buildée, vérifiée et testée sur ordinateur, poussée sur la branche **`v4`** (commit `177b18a`). **Pas encore essayée sur l'unité.** `main` reste en v3.

## 1. Ce qui change, v3 → v4

| | v3 (forme 2) | v4 (forme 3) |
|---|---|---|
| EQ, effet (8 valeurs), compresseur, vitesse | oui | oui |
| type de l'effet master (`fx_type`) | oui | oui |
| interrupteur de l'effet (`fx_active`) | non | **oui** |
| effet de pack posé sur le master (`fx_effect`) | non | **oui** |
| mémoires M1/M2 (`m1`, `m2`) | non | **oui** |
| lecture des formes précédentes | 1 | 1 et 2 |

**Taille et contenu du pack :**
- 9 812 octets, SHA-256 `08085ba94186f94e8c4e582da22fc719db01db6b6be497c4de0b6137534eab4d` ;
- niveau 7, 0 modification du firmware, 1 demande `keeps` ;
- noms demandés au base : `op1f_level_7`, `op1f_log`, `op1f_value_get`, `op1f_value_set`, `op1f_rack_at`, `op1f_rack_list`, tous offerts par le base 30. Aucun appel à une bibliothèque.

## 2. Points de rencontre nouveaux

| Adresse ou nom | Rôle | Usage |
|---|---|---|
| `0x8d50424c` | l'effet master est-il allumé (sans argument, 0 ou 1) : la réponse du firmware, sous celle du base | lu à chaque frame |
| `0x8d5042c0` | allume ou éteint l'effet master `(on)` | appelé au chargement, après l'effet |
| `0x849a8a10`, `0x849a8a38` | mémoires M1 et M2, 40 octets chacune | lues et écrites (voir `reverse-tricks-m1m2-2026-11-23.md`) |
| `0x8c0ba620` | accesseur du contrôleur du mixer | appelé pour poser un effet de pack |
| mot `+0x28` de la table du contrôleur | le choix d'un effet de la liste. Le base s'y place (`op1f_fx_master_pick`) | appelé avec l'`id` de l'effet de pack dans la liste du base |
| `op1f_rack_at`, `op1f_rack_list` | API du base : quel effet de pack est posé sur le master, et la liste des effets de packs | lus à chaque frame |

## 3. Écarts par rapport au briefing

1. **`op1f_fx_master_pick` n'est pas un nom que le base offre.** Il ne figure pas parmi les 59 `exports` de `packs/base.json`. Un pack qui le déclarerait en `extern` demanderait un nom que personne n'offre, et le loader l'écarterait au boot. La v4 l'atteint autrement : le base place cette fonction dans le mot `+0x28` de la table du contrôleur du mixer, et le pack appelle ce mot tel que l'unité le contient. La fonction appelée est la même (Route A), sans dépendance de lien. Pour un effet de pack, l'`id` attendu est bien celui de la liste du base (15 et plus). Le piège de la v2, qui passait un type au lieu d'un `id`, ne s'applique pas ici.
2. **Mettre fin à un effet de pack posé sur le même type.** Une cassette sans `fx_effect`, chargée alors qu'un effet de pack est posé sur le type même qu'elle garde (par exemple `ping` sur `delay`, la cassette voulant `delay` seul) : changer le type ne suffit pas, puisqu'il ne change pas, et c'est en voyant le type changer que le base retire l'effet de pack. Le pack pose donc un autre type pendant une frame, attend que le base ait retiré l'effet (au plus 4 frames), puis pose le type de la cassette et le reste. Ce cas n'était pas dans le briefing.
3. **Layout des mémoires.** `has_data` est un **octet** à `+4` (booléen dans `op1.json`), et non un mot. `subscreen` est un mot à `+0`, et non à la fin. Les valeurs sont à `+8`. Le pas de 40 octets est confirmé.
4. **M1/M2 pour les formes 1 et 2.** Le briefing se contredisait : le §4.3 demandait de garder les mémoires actuelles, le §5.1 de reprendre celles du départ. La v4 garde les mémoires telles qu'elles sont, conformément à la justification du §4.3. L'interrupteur, lui, reprend la valeur du départ (§4.3).
5. **Entrée catalogue.** `needs` est `[]`, et non `["base"]` (voir `catalog-entry-save-mod.md`).
6. **`has_data` et `fx_active`** acceptent aussi `true` et `false` à la lecture. Ils sont écrits en 0 ou 1.

## 4. Tests faits (sur ordinateur)

**Forme du membre (15 cas, tous bons) :**
- aller-retour forme 3, avec et sans `fx_effect` ni `fx_type` ;
- lecture des formes 1 et 2 ;
- refus d'une forme 2 contenant `fx_active`, et d'une forme 1 contenant `fx_type` ;
- édition hostile acceptée et corrigée : `fx_active: 99` devient 1, une vitesse de 999999 est ramenée à +12, `params` à ±99999 est ramené à ±32767, `subscreen: 42` devient 0 ;
- refus d'une mémoire incomplète, d'une forme 3 sans `fx_active`, d'un nom d'effet invalide, d'un membre en double, et de la version 4 ;
- pire cas : 429 octets sur les 640 de la zone de texte.

**Pose de l'effet (7 scénarios, contre des simulations du firmware et du base, tous bons) :**
- pose d'un effet de pack, sans rien refaire s'il est déjà posé ;
- retour à un effet stock d'un autre type ;
- fin d'un effet de pack sur le même type en deux frames, y compris quand le base ne le retire pas ;
- effet de pack absent : repli sur le type stock ;
- lecture du nom de l'effet de pack posé.

**Assembleur :** relu pour l'appel du choix (contrôleur dans R0, `id` dans R1, mot `+0x28`).

**Contrôles :** `pack_build.py check`, `publish_check.py --stock` (rien du firmware dans le dépôt), `catalog_check.py` sur le brouillon d'entrée.

## 5. À essayer sur l'unité (test de recette §7 du briefing)

- **`fx_active` :** cassette 1 effet allumé, cassette 2 éteint. L'interrupteur doit suivre la cassette.
- **`fx_effect` :**
  - poser un wagon de `fx train` ou `ping` sur le master, changer de cassette puis revenir : l'effet de pack doit revenir avec ses valeurs ;
  - essayer aussi une cassette avec le même effet stock sans effet de pack : l'effet de pack doit partir. C'est le cas en deux frames, avec un bref changement d'effet audible possible.
- **M1/M2 :**
  - prendre M1 sur la page EQ et M2 sur la page effet ;
  - changer de cassette : les mémoires doivent suivre ;
  - revenir, puis appuyer court sur M1 : l'EQ doit revenir.
- **Rétrocompatibilité :** une cassette v3 (forme 2) doit se charger sans changer les mémoires.
- **Dans le log :** pas de `Master fx param out of range`, ni de ligne `save_mod: the tape's effect '…' is not here` quand le pack de l'effet est installé.

## 6. Ensuite

1. Essai de la v4 sur l'unité.
2. Si tout est bon : fusionner `v4` dans `main`, puis ouvrir la PR catalogue avec `catalog-entry-save-mod.md`. Si un fichier change entre-temps, mettre à jour `commit`, `sha256` et `bytes`.
3. Optionnel : une PR upstream pour qu'open-field expose un choix d'effet de pack sur le master dans son API publique.
