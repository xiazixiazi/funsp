
## Requirements

```shell
apt update
apt install -y gcc make python3 python3-pip
pip install -r requirements.txt
```

**​Prerequisite: LLVM Environment Setup​​**

Ensure the LLVM compilation environment is properly configured before proceeding:

```shell
# Install essential dependencies (Ubuntu example)
sudo apt-get install build-essential cmake ninja-build

# Build LLVM from source (recommended for pass development)
git clone https://github.com/llvm/llvm-project.git
cd llvm-project && mkdir build && cd build
cmake -G Ninja ../llvm -DLLVM_ENABLE_PROJECTS="clang"
ninja
```

## Usage

​**Compilation Guide​​**

1. ​​Build Shared Library​​

    Use this command to compile the func_split_pass.so file:

    ```shell
    clang++ -I/path/to/llvm/include -shared -fPIC \
    `llvm-config --cxxflags --ldflags --system-libs --libs core` \
    ./func_split_pass.cpp -o ./func_split_pass.so
    ```

2. Configure Absolute Path​​

    Update the PASS_PATH variable in ./custom_compiler.py with your actual absolute path:

    ```python
    # Example path, modify according to your actual setup
    PASS_PATH = "/home/test/my_lib/my_paper/func_split/demo/func_split_pass/func_split_pass.so"
    ```

3. Compile Target Code​​

    Execute the custom compiler script with your C source file:
    
    ```shell
    ./custom_compiler.py your_target_code.c
    ```

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
