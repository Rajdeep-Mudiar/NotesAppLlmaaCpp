import os
import sys
import json
import warnings
from pymongo import MongoClient
from bson import ObjectId
from dotenv import load_dotenv

# Silence all warnings to prevent breaking the C++ JSON parser
warnings.filterwarnings("ignore")
import logging
logging.getLogger("pymongo").setLevel(logging.ERROR)

# Load .env from the script's directory
script_dir = os.path.dirname(os.path.abspath(__file__))
load_dotenv(os.path.join(script_dir, ".env"))

MONGO_URL = os.getenv("MONGO_DB_URL")
if not MONGO_URL:
    print(json.dumps({"error": "MONGO_DB_URL not found in .env"}))
    sys.exit(1)

client = MongoClient(MONGO_URL)
db = client["second_brain"]

def item_to_dict(item):
    item["id"] = str(item.pop("_id"))
    return item

def list_items(coll_name):
    items = list(db[coll_name].find({}))
    return [item_to_dict(n) for n in items]

def save_item(coll_name, data):
    if "id" in data and data["id"]:
        item_id = data.pop("id")
        db[coll_name].replace_one({"_id": ObjectId(item_id)}, data, upsert=True)
        return str(item_id)
    else:
        result = db[coll_name].insert_one(data)
        return str(result.inserted_id)

def delete_item(coll_name, item_id):
    db[coll_name].delete_one({"_id": ObjectId(item_id)})
    return True

if __name__ == "__main__":
    # Force UTF-8 for all stdout/stderr on Windows
    if sys.platform == "win32":
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

    if len(sys.argv) < 3:
        sys.exit(1)

    cmd = sys.argv[1]
    coll_name = sys.argv[2]
    
    try:
        if cmd == "list":
            # Use ensure_ascii=True to avoid 'charmap' errors on Windows pipes
            # nlohmann::json in C++ will correctly parse the \uXXXX sequences.
            print(json.dumps(list_items(coll_name), ensure_ascii=True))
        elif cmd == "save_file":
            with open(sys.argv[3], 'r', encoding='utf-8') as f:
                data = json.load(f)
            new_id = save_item(coll_name, data)
            print(json.dumps({"id": new_id}))
        elif cmd == "delete":
            delete_item(coll_name, sys.argv[3])
            print(json.dumps({"success": True}))
    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)
