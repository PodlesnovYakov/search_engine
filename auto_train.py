import requests
import csv
import random
import sys

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

def evaluate(dataset, k1, b, w_title):
    hits = 0
    total = 0
    
    for target_id, title in dataset:
        try:
            resp = requests.get(SERVER_URL, params={
                "q": title, 
                "k1": k1, 
                "b": b, 
                "w_title": w_title
            }, timeout=1)
            
            if resp.status_code != 200: continue
            results = resp.json()
            if not results: continue

            total += 1
            if results[0]['id'] == target_id:
                hits += 1
            elif results[0]['title'].strip().lower() == title.strip().lower():
                hits += 1
        except:
            continue

    if total == 0: return 0.0
    return (hits / total) * 100

def train():
    full_data = load_dataset()
    if not full_data: return

    sample = random.sample(full_data, NUM_SAMPLES)
    print("Starting Grid Search...")

    best_score = -1.0
    best_params = (0, 0, 0)

    w_title_values = [1.0, 5.0, 10.0] 
    k1_values = [1.2, 1.5, 2.0]            
    b_values = [0.4, 0.75, 1.0]            

    print(f"{'w_title':<8} | {'k1':<6} | {'b':<6} | {'Accuracy':<10}")
    print("-" * 40)

    for w in w_title_values:
        for k1 in k1_values:
            for b in b_values:
                score = evaluate(sample, k1, b, w)
                print(f"{w:<8.1f} | {k1:<6.1f} | {b:<6.2f} | {score:.1f}%")
                
                if score > best_score:
                    best_score = score
                    best_params = (w, k1, b)

    print("-" * 40)
    print(f"BEST RESULT: Accuracy {best_score:.1f}%")
    print(f"PARAMS: w_title={best_params[0]}, k1={best_params[1]}, b={best_params[2]}")

if __name__ == "__main__":
    train()