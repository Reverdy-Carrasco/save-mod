# save mod v5 — travail en cours

Ce dossier tient la v5, séparée du pack publié. `examples/save-mod/` et `out/save mod.pack` (v4, au catalogue open-field) n'y sont pas touchés.

## Contenu

| Chemin | Quoi |
|---|---|
| `docs/sonde-niveau3-2026-11-28.md` | Sonde de l'API fichiers niveau 3 sur base 44 (0.26) : lecture du source, tests, grille de décision |
| `examples/save-sonde/` | Sources du pack sonde (card niveau 3, aucune modif firmware) |
| `out/save-sonde.pack` | Le pack sonde construit (9 528 oct, SHA-256 `cc69400f8141dbe00f61421febcf341430fd634e9b3c9353add437565582b5ed`). Pas encore essayé sur une unité |

## Rebuild du pack sonde

Dans un clone de `LaP0573/open-field` au commit `893e59f`, avec la toolchain en place (`docs/build.md` d'open-field), copier `examples/save-sonde/` dans `examples/save-sonde/` du clone, puis :

```bash
python -I tools/pack_build.py card --recipe examples/save-sonde/recipe.json --out examples/save-sonde/save-sonde_card.s
```

```bash
python -I tools/pack_build.py build3 --recipe examples/save-sonde/recipe.json --profile profiles/op1f-1.7.3.json --out out/save-sonde.pack
```

```bash
python -I tools/pack_build.py check --pack out/save-sonde.pack
```
