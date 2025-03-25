
## Requirements

```shell
apt update
apt install -y gcc make python3 python3-pip
pip install -r requirements.txt
```

## Usage

Datasets: https://doi.org/10.6084/m9.figshare.28660049.v1

```shell
tar -zxf datasets.tar.gz
```

Merge datasets

```shell
mkdir output
python3 merge_vector.py
```

Calcuate MRR and recall

```shell
python3 calculate_mrr_recall.py -i output/jTrans_merge_vector.pkl -p 32
python3 calculate_mrr_recall.py -i output/asm2vec_merge_vector.pkl -p 32
python3 calculate_mrr_recall.py -i output/SAFE_merge_vector.pkl -p 32
```

Compare the running time

```shell
make
./test_time_consumption
```

Get jTrans similarity between `O0-sub-fla-bcf` and `clang_O0_split_mean` against to `clang_O0`

```shell
python3 get_jTrans_similarity.py
```

Measure the file sizes of the following binaries: `O0-sub-fla-bcf`, `clang_O0_split_mean` and `clang_O0`

```shell
python3 compare_filesize.py
```

Generate svg image

```shell
python3 generate_image_split_vs_ollvm.py
```
