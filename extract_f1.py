import fastf1
import pandas as pd
import os

#creation of a folder name cahche_f1 for the fastf1 api cache
cache_dir = 'cache_f1'

if not os.path.exists(cache_dir):
    os.makedirs(cache_dir)

fastf1.Cache.enable_cache(cache_dir)

#chose the circuit and R for the Race or Q for the Qualification 
session = fastf1.get_session(2023, 'Monza', 'R')
session.load()

#Take the fastest time of a pilote in one lap EX her VER for Verstapen
lap = session.laps.pick_drivers('VER').pick_fastest()

telemetry = lap.get_telemetry()

df = telemetry[['Time', 'Speed', 'RPM', 'nGear', 'Throttle', 'Brake']].copy()

#Format the time for C++ model
df['Time_ms'] = df['Time'].dt.total_seconds() * 1000

#Delete the older time columns
df = df.drop(columns=['Time'])

#save the file
output_filename = 'telemetry_VER_Monza.csv'
df.to_csv(output_filename, index=False)

print(f"Sucess ! The file: {output_filename} load with {len(df)} line of data.")