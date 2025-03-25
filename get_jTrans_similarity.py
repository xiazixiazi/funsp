
import os
import pandas as pd
import time
from tqdm import tqdm
import logging
import math
import numpy as np

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

logging.info("Start")
start = time.time()

path_prefix = 'datasets/coreutils-8.30'
model = 'jTrans'

type_list = [
    'clang_O0',
    'clang_O0_split_mean',
]

bin_list = ['mv', 'fold', 'nproc', 'groups', 'shred', 'sha384sum', 'whoami', 'printenv', 'mktemp', 'du', 'uname', 'sha1sum', 'hostid', 'od', 'sum', 'unlink', 'basename', 'chgrp', 'nice', 'pinky', 'kill', 'mkdir', 'rm', 'sha256sum', 'sort', 'false', 'cksum', 'split', 'sleep', 'who', 'test', 'chcon', 'tr', 'logname', 'truncate', 'ln', 'stat', 'df', 'numfmt', 'dd', 'fmt', 'users', 'stty', 'chmod', 'tac', 'md5sum', 'nohup', 'uptime', 'csplit', 'timeout', 'paste', 'echo', 'pr', 'chown', 'env', 'ptx', 'mknod', 'sha224sum', 'nl', 'mkfifo', 'ls', 'shuf', 'true', 'cut', 'unexpand', 'comm', 'head', 'base32', 'dircolors', 'chroot', 'runcon', 'dirname', 'seq', 'printf', 'link', 'tty', 'yes', 'id', 'pwd', 'touch', 'vdir', 'join', 'wc', 'realpath', 'base64', 'b2sum', 'factor', 'dir', 'expand', 'uniq', 'cp', 'stdbuf', 'date', 'cat', 'sync', 'sha512sum', 'tail', 'rmdir', 'tsort', 'readlink', 'expr', 'pathchk', 'tee']

dataset = {}
for _bin in bin_list:
    dataset[_bin] = {}
    for _type in type_list:
        df = pd.read_pickle(f'{path_prefix}/{_type}/{_bin}_{model}_vector.pkl')
        dataset[_bin][_type] = df

bypass_list = [
    '_start',
    '_dl_relocate_static_pie',
    'deregister_tm_clones',
    'register_tm_clones',
    '__do_global_dtors_aux',
    'frame_dummy',
]

obfuscator_list = [
    "O0-sub-fla-bcf",
]

for _bin in bin_list:
    for obfuscator_type in obfuscator_list:
        df = pd.read_pickle(f'{path_prefix}/obfuscator-llvm-{obfuscator_type}/{_bin}_{model}_vector.pkl')
        dataset[_bin][obfuscator_type] = df

new_dataset = []
for _bin in tqdm(bin_list):
    funcname_df = dataset[_bin][type_list[0]]
    funcname_list = funcname_df["funcname"]
    max_list = []
    avg_list = []
    min_list = []
    obfuscator_dict = {}
    for obfuscator_type in obfuscator_list:
        obfuscator_dict[obfuscator_type] = []

    for funcname in funcname_list:

        if funcname in bypass_list:
            continue

        if str(funcname) == 'nan':
            continue

        df = dataset[_bin][type_list[0]]
        origin = df[df["funcname"] == funcname]
        origin_vector = math.nan
        if len(origin) > 0:
            origin_vector = origin["vector"].iloc[0]
        else:
            continue

        df = dataset[_bin][type_list[1]]
        split = df[df["funcname"] == funcname]
        split_vector = math.nan
        if len(split) > 0:
            split_vector = split["vector"].iloc[0]
        else:
            continue

        splitFlag_name = funcname + '_splitFlag'

        df = dataset[_bin][type_list[1]]
        splitFlag = df[df["funcname"] == splitFlag_name]
        splitFlag_vector = math.nan
        if len(splitFlag) > 0:
            splitFlag_vector = splitFlag["vector"].iloc[0]
        else:
            continue

        target = np.array(origin_vector).squeeze()
        contrastives1 = np.array(split_vector).squeeze()
        contrastives2 = np.array(splitFlag_vector).squeeze()

        target_norm = target / np.linalg.norm(target)
        contrastives1_norm = contrastives1 / np.linalg.norm(contrastives1)
        contrastives2_norm = contrastives2 / np.linalg.norm(contrastives2)

        similarity1 = np.dot(contrastives1_norm, target_norm)
        similarity2 = np.dot(contrastives2_norm, target_norm)

        max_list += [np.max([similarity1, similarity2])]
        avg_list += [np.mean([similarity1, similarity2])]
        min_list += [np.min([similarity1, similarity2])]

        obfuscator_loop_success = True
        similarity_list = {}
        for obfuscator_type in obfuscator_list:
            df = dataset[_bin][obfuscator_type]
            obfuscator = df[df["funcname"] == funcname]
            obfuscator_vector = math.nan
            if len(obfuscator) > 0:
                obfuscator_vector = obfuscator["vector"].iloc[0]
            else:
                obfuscator_loop_success = False
                continue

            contrastives = np.array(obfuscator_vector).squeeze()
            contrastives_norm = contrastives / np.linalg.norm(contrastives)

            similarity = np.dot(contrastives_norm, target_norm)
            similarity_list[obfuscator_type] = similarity
        
        if obfuscator_loop_success == False:
            continue

        for obfuscator_type in obfuscator_list:
            obfuscator_dict[obfuscator_type].append(similarity_list[obfuscator_type])
        
    one_item = [_bin, np.mean(max_list), np.mean(avg_list), np.mean(min_list)]
    for obfuscator_type in obfuscator_list:
        one_item += [np.mean(obfuscator_dict[obfuscator_type])]
    new_dataset += [one_item]

df = pd.DataFrame(new_dataset, columns=["program","split-O0-max", "split-O0-avg", "split-O0-min"] + obfuscator_list)

for a in obfuscator_list:
    for b in ["split-O0-max", "split-O0-avg", "split-O0-min"]:
        df[f'{a}.abs.{b}'] = (df[a] - df[b]).abs()

df.to_csv(f'output/jTrans_similarity_result.csv', index=False)
logging.info(f"Total: {len(new_dataset)}")

end = time.time()
logging.info(f"[*] Time Cost: {end - start} seconds")
