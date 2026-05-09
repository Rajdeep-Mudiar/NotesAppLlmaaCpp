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
collection = db["notes"]

def note_to_dict(note):
    note["id"] = str(note.pop("_id"))
    return note

def list_notes():
    notes = list(collection.find({}))
    return [note_to_dict(n) for n in notes]

def save_note(note_data):
    if "id" in note_data and note_data["id"]:
        note_id = note_data.pop("id")
        collection.replace_one({"_id": ObjectId(note_id)}, note_data, upsert=True)
        return str(note_id)
    else:
        result = collection.insert_one(note_data)
        return str(result.inserted_id)

def delete_note(note_id):
    collection.delete_one({"_id": ObjectId(note_id)})
    return True

if __name__ == "__main__":
    # Force UTF-8 for all stdout/stderr on Windows
    if sys.platform == "win32":
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

    if len(sys.argv) < 2:
        sys.exit(1)

    cmd = sys.argv[1]
    
    try:
        if cmd == "list":
            # Use ensure_ascii=True to avoid 'charmap' errors on Windows pipes
            # nlohmann::json in C++ will correctly parse the \uXXXX sequences.
            print(json.dumps(list_notes(), ensure_ascii=True))
        elif cmd == "save_file":
            with open(sys.argv[2], 'r', encoding='utf-8') as f:
                data = json.load(f)
            new_id = save_note(data)
            print(json.dumps({"id": new_id}))
        elif cmd == "delete":
            delete_note(sys.argv[2])
            print(json.dumps({"success": True}))
    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)
