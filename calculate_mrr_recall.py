
import pandas as pd
import numpy as np
from tqdm import tqdm
import logging
import argparse
import time

def compute_metrics(samples, ks=[1, 2, 5, 10]):
    """
    Calculate MRR and Recall@k for multiple samples
    
    Parameters:
    samples - list of samples, each sample contains:
        target_vector: target vector (np.ndarray)
        contrastive_vectors: contrast vector matrix (np.ndarray, shape=(32, dim))
        correct_indices: list of indices of correct vectors (list, length=2)
    ks - list of k values to be calculated
    
    Returns:
    avg_mrr - average MRR
    avg_recalls - average Recall for each k value
    """
    mrr_list = []
    recall_dict = {k: [] for k in ks}
    
    for sample in tqdm(samples):
        target = sample['target_vector']
        contrastives = sample['contrastive_vectors']
        correct_indices = sample['correct_indices']
        
        # Dimension adjustment
        if len(target.shape) > 1:
            target = target.squeeze()  # From (1,vector) to (vector,)
        if len(contrastives.shape) > 2:
            contrastives = contrastives.squeeze(axis=1)  # From (pool,1,vector) to (pool,vector)

        # Normalization
        target_norm = target / np.linalg.norm(target)
        contrastives_norm = contrastives / np.linalg.norm(contrastives, axis=1, keepdims=True)
        
        # Calculate cosine similarity
        cos_sims = np.dot(contrastives_norm, target_norm)
        
        # Get the indexes sorted in descending order of similarity
        sorted_indices = np.argsort(cos_sims)[::-1]

        # Calculate the ranking of the correct vector
        ranks = []
        for idx in correct_indices:
            rank = np.where(sorted_indices == idx)[0][0] + 1  # 1-based ranking
            ranks.append(rank)
        
        # Calculating MRR
        min_rank = min(ranks)
        mrr_list.append(1.0 / min_rank)
        
        # Calculate the recall for each k value
        for k in ks:
            top_k = set(sorted_indices[:k])
            hits = len(top_k & set(correct_indices))
            recall = hits / len(correct_indices)
            recall_dict[k].append(recall)
    
    # Calculate the average
    avg_mrr = np.mean(mrr_list)
    avg_recalls = {k: float(np.mean(v)) for k, v in recall_dict.items()}
    
    return avg_mrr, avg_recalls

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

    logging.info("Start")
    start = time.time()

    parser = argparse.ArgumentParser(description='Evaluation.')
    parser.add_argument('-i', '--input', type=str, required=True, help="The input of merge vector datasets.")
    parser.add_argument('-p', '--pool', type=int, default=32, required=False, help="The pool of evaluation")
    parser.add_argument('-d', '--debug', action='store_true', help="Enable verbose output")
    args = parser.parse_args()

    if args.debug:
        logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
    else:
        logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

    pool = args.pool
    logging.info(f'pool: {pool}')

    datasets_df = pd.read_pickle(args.input)

    type_list = {
        "O0-O0_split":  {"target":"O0", "match":["O0_split", "O0_splitFlag"]},
        "O1-O1_split":  {"target":"O1", "match":["O1_split", "O1_splitFlag"]},
        "O2-O2_split":  {"target":"O2", "match":["O2_split", "O2_splitFlag"]},
        "O3-O3_split":  {"target":"O3", "match":["O3_split", "O3_splitFlag"]},
        "O0-O3":        {"target":"O0", "match":["O3"]},
        "O0-O3_split":  {"target":"O0", "match":["O3_split", "O3_splitFlag"]},
    }

    headless_df = datasets_df.drop('funcname', axis=1)

    input_data = {}
    for _type in type_list:
        input_data[_type] = []

    logging.info('Sample')
    for item_index in tqdm(range(len(datasets_df))):
        item = datasets_df.iloc[item_index]
        tmp_df = headless_df.drop(item_index)
        for _type in type_list:
            target_type = type_list[_type]["target"]
            target = item[target_type]
            prefix_list = []
            has_nan = False
            for bin_type in type_list[_type]["match"]:
                bin_vector = item[bin_type]
                if isinstance(bin_vector, float):
                    has_nan = True
                    break
                prefix_list += [bin_vector]

            if isinstance(target, float) or has_nan == True:
                continue

            tmp_df_filtered = tmp_df.dropna(subset=[target_type])

            random_data = tmp_df_filtered.sample(n=pool-len(prefix_list))[target_type].tolist()
            evaluation_data = np.array(prefix_list + random_data)
            input_data[_type] += [{"target_vector":np.array(target), "contrastive_vectors":evaluation_data, "correct_indices":[i for i in range(len(prefix_list))]}]

    logging.info('compute_metrics start')
    MRR_list = []
    R1_list = []
    for _type in type_list:
        logging.info(f'compute_metrics {_type}')
        avg_mrr, avg_recalls = compute_metrics(input_data[_type])
        logging.info(f'avg_mrr: {avg_mrr}, avg_recalls: {avg_recalls}')
        MRR_list += [avg_mrr]
        R1_list += [avg_recalls[1]]
    end = time.time()
    logging.info(f"MRR_avg: {np.mean(MRR_list)}, R1_avg: {np.mean(R1_list)}")
    logging.info(f"[*] Time Cost: {end - start} seconds")
