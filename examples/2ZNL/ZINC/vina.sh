for f in ../../../ligands/ZINC/*; do
	s=${f:22:${#f}-28}
	vina --config vina.conf --ligand $f --out out/$s.pdbqt --log log/$s.txt
done
