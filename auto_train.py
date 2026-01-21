import requests
import csv
import random
import sys
import time

SERVER_URL = "http://localhost:8080/search"
CSV_PATH = "data/wiki_movie_plots_deduped.csv"
NUM_SAMPLES = 100

def load_dataset():
    dataset = []
    print("Reading CSV...")
    try:
        with open(CSV_PATH, 'r', encoding='utf-8') as f:
            reader = csv.reader(f)
            next(reader)
            for idx, row in enumerate(reader):
                if len(row) >= 2:
                    dataset.append((idx, row[1])) 
    except Exception as e:
        print(f"Error: {e}")
        return []
    return dataset

def evaluate(dataset, k1, b, w_title, w_prox):
    hits_top1 = 0
    hits_top5 = 0
    reciprocal_rank_sum = 0.0
    total = 0
    
    for target_id, title in dataset:
        try:
            resp = requests.get(SERVER_URL, params={
                "q": title, 
                "k1": k1, 
                "b": b, 
                "w_title": w_title,
                "w_prox": w_prox
            }, timeout=2)
            
            if resp.status_code != 200: continue
            results = resp.json()
            if not results: continue

            total += 1
            found_rank = -1
            
            for i, res in enumerate(results):
                is_match = (res['id'] == target_id) or \
                           (res['title'].strip().lower() == title.strip().lower())
                
                if is_match:
                    found_rank = i + 1
                    break
            
            if found_rank == 1:
                hits_top1 += 1
            
            if found_rank != -1 and found_rank <= 5:
                hits_top5 += 1
                
            if found_rank != -1:
                reciprocal_rank_sum += 1.0 / found_rank

        except Exception as e:
            continue

    if total == 0: return 0.0, 0.0, 0.0
    
    accuracy = (hits_top1 / total) * 100
    recall_at_5 = (hits_top5 / total) * 100
    mrr = (reciprocal_rank_sum / total)
    
    return accuracy, recall_at_5, mrr

def train():
    full_data = load_dataset()
    if not full_data: return

    if len(full_data) < NUM_SAMPLES:
        sample = full_data
    else:
        sample = random.sample(full_data, NUM_SAMPLES)
        
    print(f"Dataset: {len(full_data)} docs. Testing on {len(sample)} random samples.")
    print("Starting Grid Search...")

    best_mrr = -1.0
    best_params = (0, 0, 0, 0)
    w_title_values = [1.0, 5.0, 10.0] 
    k1_values = [1.2]            
    b_values = [0.75]             
    w_prox_values = [0.0, 1.0, 5.0]       

    print(f"{'w_tit':<5} | {'k1':<4} | {'b':<4} | {'w_prx':<5} || {'Acc':<6} | {'R@5':<6} | {'MRR':<5}")
    print("-" * 65)

    for w in w_title_values:
        for k1 in k1_values:
            for b in b_values:
                for w_prox in w_prox_values: 
                    acc, r5, mrr = evaluate(sample, k1, b, w, w_prox)
                    
                    print(f"{w:<5.1f} | {k1:<4.1f} | {b:<4.2f} | {w_prox:<5.1f} || {acc:<5.1f}% | {r5:<5.1f}% | {mrr:.3f}")
                    if mrr > best_mrr:
                        best_mrr = mrr
                        best_params = (w, k1, b, w_prox)

    print("-" * 65)
    print(f"BEST RESULT (by MRR): {best_mrr:.3f}")
    print(f"PARAMS: w_title={best_params[0]}, k1={best_params[1]}, b={best_params[2]}, w_prox={best_params[3]}")

if __name__ == "__main__":
    train()