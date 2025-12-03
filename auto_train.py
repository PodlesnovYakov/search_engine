import requests
import csv
import random
import sys

SERVER_URL = "http://localhost:8080/search"
CSV_PATH = "data/wiki_movie_plots_deduped.csv"
NUM_SAMPLES = 50 

def load_dataset():
    dataset = []
    print("Reading CSV...")
    try:
        with open(CSV_PATH, 'r', encoding='utf-8') as f:
            reader = csv.reader(f)
            next(reader) # Skip header
            for idx, row in enumerate(reader):
                if len(row) >= 2:
                    # Сохраняем: (ID документа, Название)
                    dataset.append((idx, row[1])) 
    except Exception as e:
        print(f"Error: {e}")
        return []
    return dataset

def evaluate(dataset, k1, b):
    hits = 0
    total = 0
    
    print(f"\n--- START EVALUATION (k1={k1}, b={b}) ---")
    
    for target_id, title in dataset:
        # [DEBUG] Печатаем запрос перед отправкой
        # flush=True заставляет питон вывести текст немедленно, не буферизуя
        print(f"[DEBUG] Querying: '{title}' (ID: {target_id})... ", end="", flush=True)
        
        try:
            resp = requests.get(SERVER_URL, params={"q": title, "k1": k1, "b": b}, timeout=5)
            
            if resp.status_code != 200:
                print(f"FAIL (HTTP {resp.status_code})")
                continue
            
            results = resp.json()
            if not results:
                print("FAIL (Empty results)")
                continue

            total += 1
            
            # Проверяем результат
            is_hit = False
            first_res_id = results[0].get('id', -1)
            first_res_title = results[0].get('title', '???')

            # Сравнение по ID (надежно)
            if first_res_id == target_id:
                is_hit = True
            # Сравнение по названию (фолбек)
            elif first_res_title.strip().lower() == title.strip().lower():
                is_hit = True
                
            if is_hit:
                hits += 1
                print("HIT!")
            else:
                print(f"MISS (Got: '{first_res_title}', ID: {first_res_id})")

        except Exception as e:
            # Если сервер упал, requests выкинет исключение ConnectionError
            print(f"\n[CRITICAL] EXCEPTION: {e}")
            print(f"[CRITICAL] SERVER PROBABLY CRASHED ON QUERY: '{title}'")
            # Прерываем тест, так как сервер скорее всего мертв
            sys.exit(1)

    if total == 0: return 0.0
    return (hits / total) * 100

def train():
    full_data = load_dataset()
    if not full_data: return

    print(f"Total movies: {len(full_data)}")
    sample = random.sample(full_data, NUM_SAMPLES)
    print(f"Validation set size: {len(sample)}")

    best_score = -1
    best_params = (0, 0)

    k1_vals = [0.5, 1.2, 2.0]
    b_vals = [0.1, 0.75, 1.0]

    for k1 in k1_vals:
        for b in b_vals:
            score = evaluate(sample, k1, b)
            print(f"Result: k1={k1}, b={b} -> Accuracy: {score:.1f}%")
            
            if score > best_score:
                best_score = score
                best_params = (k1, b)
    
    print("-" * 30)
    print(f"Best: k1={best_params[0]}, b={best_params[1]} (Acc: {best_score:.1f}%)")

if __name__ == "__main__":
    train()