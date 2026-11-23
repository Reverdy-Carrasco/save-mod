# Brouillon d'entrée catalogue — `save mod` 4

**Où elle va :** `catalog/packs.json` d'open-field, à la fin de `packs`, dans une pull request qui ne change que ce fichier (`docs/catalog.md` d'open-field).
**Quand :** à ouvrir seulement après l'essai de la v4 sur l'unité. Pas encore ouverte.

```json
{
  "id": "save mod",
  "author": "Reverdy-Carrasco",
  "status": "listed",
  "categories": ["tape", "mixer"],
  "description": "Keeps per tape the master EQ, effect (with its switch and a pack's effect on it), compressor, tape speed and M1/M2.",
  "repository": "https://github.com/Reverdy-Carrasco/save-mod",
  "commit": "177b18a5f412ea7f96d0eea297d5a931a7f8a25b",
  "path": "out/save mod.pack",
  "sha256": "08085ba94186f94e8c4e582da22fc719db01db6b6be497c4de0b6137534eab4d",
  "bytes": 9812,
  "version": 4,
  "firmware": "1.7.3",
  "places": 0,
  "needs": [],
  "level": 7,
  "folders": []
}
```

**Vérifications faites :** `tools/catalog_check.py` sur une copie du catalogue avec cette entrée ajoutée.
- Avec `--files` et le pack : `ok: save mod (listed, its file held)`. La taille, le SHA-256, la version, le firmware, les emplacements, les dépendances, le niveau et les dossiers correspondent au fichier.
- Avec `--before`, contre le catalogue actuel : `16 entries, all in order`.

**À savoir :**
- **`needs` est vide.** Dans le catalogue, il liste les autres packs dont celui-ci a besoin. Le base n'y figure pas, puisque le champ `level` le couvre. C'est ce que fait chaque entrée actuelle, et l'outil confronte ce champ au fichier. `["base"]` serait donc refusé.
- **La description** fait 115 caractères ; la limite est de 140.
- **`commit`** est le commit qui porte ce fichier du pack. Si la v4 change avant la PR (après l'essai sur l'unité), il faut mettre à jour `commit`, `sha256` et `bytes`, puis relancer `catalog_check.py`.
