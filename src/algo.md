pixels <=> clusters

créer tableau suffisamment grand
[edge node..... | corner nodes]
 ^                ^
 0                4 * arity * n_pixels

for each pixel
    for each edge
        if onborder
            stocker edges
            ajouter edge nodes
            ajouter corner nodes avec (x, y)

for each p1 in edge nodes
    for each p2 in edge nodes
        si p1 et p2 proches, union(p1, p2)

for each p1 in corner nodes
    for each p2 in corner nodes
        if p1 et p2 proches and no_diagonal_clusters
            union(p1, p2)


comprimer union find (compteur + reverse mapping)

appliquer l'union find compressé aux nodes, puis aux edges
fusionner les edges qui ont les même nodes

creer liste [i: cluster id] -> liste d'edge
pour calculer l'aire, on fabrique une liste d'adjacence a partir de la liste eparpillee d'edges
parcours de graphe (bfs dfs osef) -> edge:
    c1, c2 = clusters adjacents(edge)
    liste[c1].push(edge)
    liste[c2].push(edge)
^ une seule fois

a chaque update des positions, recalculer l'aire:
triangle par triangle (triangle strip), somme des aires des triangles