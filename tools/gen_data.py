"""Generates a reproducible demo dataset (Noida area). Run from repo root: python tools/gen_data.py"""
import csv, random
random.seed(2026)
LAT, LON = (28.50, 28.65), (77.30, 77.45)
def pt(): return round(random.uniform(*LAT), 5), round(random.uniform(*LON), 5)

inc_types = {"FLOOD": ["Waterlogging on main road", "River overflow near colony"],
             "FIRE": ["Warehouse fire", "Residential building fire", "Market stall fire"],
             "EARTHQUAKE": ["Tremor damage reported", "Wall collapse after tremor"],
             "ACCIDENT": ["Highway multi-vehicle collision", "Bus overturned"],
             "MEDICAL": ["Mass food poisoning", "Cardiac emergency at stadium"],
             "COLLAPSE": ["Under-construction building collapse", "Bridge slab collapse"]}
with open("data/incidents.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["id","type","severity","lat","lon","reported_at","status","description"])
    for i in range(1, 51):
        t = random.choice(list(inc_types)); la, lo = pt()
        w.writerow([i, t, random.randint(1, 10), la, lo, 1000 + i * random.randint(2, 9), "OPEN", random.choice(inc_types[t])])

rtypes = {"AMBULANCE": ["ICU", "BASIC_LIFE_SUPPORT"], "FIRE_TRUCK": ["HIGH_RISE", "WATER_TANKER"],
          "RESCUE_TEAM": ["HEAVY_LIFT", "SEARCH_DOGS"], "BOAT": ["WATER_RESCUE"]}
with open("data/resources.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["id","name","type","capability","lat","lon","status","avail_from","avail_to"])
    for i in range(1, 31):
        t = random.choice(list(rtypes)); la, lo = pt(); s = random.randint(900, 1100)
        w.writerow([i, f"{t.title().replace('_',' ')} {i}", t, random.choice(rtypes[t]), la, lo,
                    random.choice(["AVAILABLE"] * 4 + ["BUSY"]), s, s + random.randint(200, 800)])

names = ["City General", "Sector 62 Medical", "Fortis-type Care", "Metro Heart", "Noida District",
         "Apex Trauma", "Sunrise Clinic", "Lifeline", "Greenfield", "Central Burn Unit",
         "Riverside", "Northgate", "Eastside Maternity", "Unity", "Prime Emergency"]
with open("data/hospitals.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["id","name","lat","lon","total_beds","free_beds"])
    for i, n in enumerate(names, 1):
        la, lo = pt(); tot = random.randint(50, 400)
        w.writerow([i, n + " Hospital", la, lo, tot, random.randint(0, tot // 4)])
print("data/ written")
