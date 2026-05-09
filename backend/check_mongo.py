import os
from pymongo import MongoClient
from dotenv import load_dotenv

script_dir = os.path.dirname(os.path.abspath(__file__))
load_dotenv(os.path.join(script_dir, ".env"))

MONGO_URL = os.getenv("MONGO_DB_URL")
client = MongoClient(MONGO_URL)

print(f"Databases: {client.list_database_names()}")
db = client["second_brain"]
print(f"Collections: {db.list_collection_names()}")
coll = db["notes"]
print(f"Count: {coll.count_documents({})}")
for doc in coll.find().limit(2):
    print(doc)
