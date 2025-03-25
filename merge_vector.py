
import os
import pandas as pd
import time
from tqdm import tqdm
import logging
import math

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

logging.info("Start")
start = time.time()

path_prefix = 'datasets/coreutils-8.30'
model_list = ['jTrans', 'asm2vec', 'SAFE']

type_list = [
    'clang_O0',
    'clang_O1',
    'clang_O2',
    'clang_O3',
    'clang_O0_split_mean',
    'clang_O1_split_mean',
    'clang_O2_split_mean',
    'clang_O3_split_mean',
]

bin_list = ['mv', 'fold', 'nproc', 'groups', 'shred', 'sha384sum', 'whoami', 'printenv', 'mktemp', 'du', 'uname', 'sha1sum', 'hostid', 'od', 'sum', 'unlink', 'basename', 'chgrp', 'nice', 'pinky', 'kill', 'mkdir', 'rm', 'sha256sum', 'sort', 'false', 'cksum', 'split', 'sleep', 'who', 'test', 'chcon', 'tr', 'logname', 'truncate', 'ln', 'stat', 'df', 'numfmt', 'dd', 'fmt', 'users', 'stty', 'chmod', 'tac', 'md5sum', 'nohup', 'uptime', 'csplit', 'timeout', 'paste', 'echo', 'pr', 'chown', 'env', 'ptx', 'mknod', 'sha224sum', 'nl', 'mkfifo', 'ls', 'shuf', 'true', 'cut', 'unexpand', 'comm', 'head', 'base32', 'dircolors', 'chroot', 'runcon', 'dirname', 'seq', 'printf', 'link', 'tty', 'yes', 'id', 'pwd', 'touch', 'vdir', 'join', 'wc', 'realpath', 'base64', 'b2sum', 'factor', 'dir', 'expand', 'uniq', 'cp', 'stdbuf', 'date', 'cat', 'sync', 'sha512sum', 'tail', 'rmdir', 'tsort', 'readlink', 'expr', 'pathchk', 'tee']

for model in model_list:
    logging.info(f"Merge {model} datasets")
    dataset = {}
    for _bin in bin_list:
        dataset[_bin] = {}
        for _type in type_list:
            df = pd.read_pickle(f'{path_prefix}/{_type}/{_bin}_{model}_vector.pkl')
            dataset[_bin][_type] = df

    new_dataset = []

    bypass_list = [
        '_start',
        '_dl_relocate_static_pie',
        'deregister_tm_clones',
        'register_tm_clones',
        '__do_global_dtors_aux',
        'frame_dummy',
    ]

    for _bin in tqdm(bin_list):
        funcname_df = dataset[_bin]["clang_O0"]
        funcname_list = funcname_df["funcname"]
        for funcname in funcname_list:

            if funcname in bypass_list:
                continue

            if str(funcname) == 'nan':
                continue

            # clang_O0
            df = dataset[_bin]["clang_O0"]
            O0 = df[df["funcname"] == funcname]
            O0_vector = math.nan
            if len(O0) > 0:
                O0_vector = O0["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O1"]
            O1 = df[df["funcname"] == funcname]
            O1_vector = math.nan
            if len(O1) > 0:
                O1_vector = O1["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O2"]
            O2 = df[df["funcname"] == funcname]
            O2_vector = math.nan
            if len(O2) > 0:
                O2_vector = O2["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O3"]
            O3 = df[df["funcname"] == funcname]
            O3_vector = math.nan
            if len(O3) > 0:
                O3_vector = O3["vector"].iloc[0]
            else:
                continue

            # clang_O0_split
            df = dataset[_bin]["clang_O0_split_mean"]
            O0 = df[df["funcname"] == funcname]
            O0_vector_split = math.nan
            if len(O0) > 0:
                O0_vector_split = O0["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O1_split_mean"]
            O1 = df[df["funcname"] == funcname]
            O1_vector_split = math.nan
            if len(O1) > 0:
                O1_vector_split = O1["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O2_split_mean"]
            O2 = df[df["funcname"] == funcname]
            O2_vector_split = math.nan
            if len(O2) > 0:
                O2_vector_split = O2["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O3_split_mean"]
            O3 = df[df["funcname"] == funcname]
            O3_vector_split = math.nan
            if len(O3) > 0:
                O3_vector_split = O3["vector"].iloc[0]
            else:
                continue

            # clang_O0_splitFlag
            splitFlag_name = funcname + '_splitFlag'
            df = dataset[_bin]["clang_O0_split_mean"]
            O0 = df[df["funcname"] == splitFlag_name]
            O0_vector_splitFlag = math.nan
            if len(O0) > 0:
                O0_vector_splitFlag = O0["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O1_split_mean"]
            O1 = df[df["funcname"] == splitFlag_name]
            O1_vector_splitFlag = math.nan
            if len(O1) > 0:
                O1_vector_splitFlag = O1["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O2_split_mean"]
            O2 = df[df["funcname"] == splitFlag_name]
            O2_vector_splitFlag = math.nan
            if len(O2) > 0:
                O2_vector_splitFlag = O2["vector"].iloc[0]
            else:
                continue

            df = dataset[_bin]["clang_O3_split_mean"]
            O3 = df[df["funcname"] == splitFlag_name]
            O3_vector_splitFlag = math.nan
            if len(O3) > 0:
                O3_vector_splitFlag = O3["vector"].iloc[0]
            else:
                continue

            new_dataset += [[funcname, O0_vector, O1_vector, O2_vector, O3_vector, O0_vector_split, O1_vector_split, O2_vector_split, O3_vector_split, O0_vector_splitFlag, O1_vector_splitFlag, O2_vector_splitFlag, O3_vector_splitFlag]]


    df = pd.DataFrame(new_dataset, columns=["funcname","O0","O1","O2","O3","O0_split","O1_split","O2_split","O3_split","O0_splitFlag","O1_splitFlag","O2_splitFlag","O3_splitFlag"])
    df.to_pickle(f'output/{model}_merge_vector.pkl')
    logging.info(f"Total: {len(new_dataset)}")

end = time.time()
logging.info(f"[*] Time Cost: {end - start} seconds")
