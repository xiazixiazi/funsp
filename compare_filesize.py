
import os
import pandas as pd

bin_list = ['mv', 'fold', 'nproc', 'groups', 'shred', 'sha384sum', 'whoami', 'mktemp', 'du', 'uname', 'sha1sum', 'hostid', 'od', 'sum', 'unlink', 'basename', 'chgrp', 'nice', 'pinky', 'kill', 'mkdir', 'rm', 'sha256sum', 'sort', 'false', 'cksum', 'split', 'sleep', 'who', 'test', 'chcon', 'tr', 'logname', 'truncate', 'ln', 'stat', 'df', 'numfmt', 'dd', 'fmt', 'users', 'stty', 'chmod', 'tac', 'md5sum', 'nohup', 'uptime', 'csplit', 'timeout', 'paste', 'echo', 'pr', 'chown', 'env', 'ptx', 'mknod', 'sha224sum', 'nl', 'mkfifo', 'ls', 'shuf', 'true', 'cut', 'unexpand', 'comm', 'head', 'base32', 'dircolors', 'chroot', 'runcon', 'dirname', 'seq', 'printf', 'link', 'tty', 'yes', 'id', 'pwd', 'touch', 'vdir', 'join', 'wc', 'realpath', 'base64', 'b2sum', 'factor', 'dir', 'expand', 'uniq', 'cp', 'stdbuf', 'date', 'cat', 'sync', 'sha512sum', 'tail', 'rmdir', 'tsort', 'readlink', 'expr', 'pathchk', 'tee']

type_list = ['clang_O0', 'clang_O0_split_mean', 'obfuscator-llvm-O0-sub-fla-bcf']

datasets = []
for _bin in bin_list:
    line = _bin
    size_list = []
    for _type in type_list:
        size = os.path.getsize(f'datasets/coreutils-8.30/{_type}/{_bin}')
        size_list += [size]
    datasets += [[_bin] + size_list]

df = pd.DataFrame(datasets, columns=["program"] + type_list)

df.to_csv(f'output/filesize_diff.csv', index=False)
