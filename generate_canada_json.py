import json
import random
import os

def generate_canada_json(filename, target_size_mb):
    print(f"Generating {filename} with target size {target_size_mb} MB...")

    target_size_bytes = target_size_mb * 1024 * 1024

    # Start the JSON structure
    header = '{"type":"FeatureCollection","features":['
    footer = ']}'

    current_size = len(header) + len(footer)

    with open(filename, 'w') as f:
        f.write(header)

        feature_count = 0
        while current_size < target_size_bytes:
            if feature_count > 0:
                f.write(',')
                current_size += 1

            # Generate a polygon with many coordinates
            coords = []
            # Make a ring of coordinates
            base_x = random.uniform(-140, -50)
            base_y = random.uniform(40, 80)

            # Generate a batch of coordinates to reduce overhead
            points = 2000 # Number of points per polygon

            coord_strs = []
            for i in range(points):
                x = base_x + random.uniform(-0.1, 0.1)
                y = base_y + random.uniform(-0.1, 0.1)
                coord_strs.append(f"[{x:.6f},{y:.6f}]")

            coords_str = ",".join(coord_strs)

            feature_str = (
                f'{{"type":"Feature","properties":{{"name":"Region {feature_count}"}},'
                f'"geometry":{{"type":"Polygon","coordinates":[[{coords_str}]]}}}}'
            )

            f.write(feature_str)
            current_size += len(feature_str)
            feature_count += 1

            if feature_count % 100 == 0:
                print(f"Generated {feature_count} features. Current size: {current_size / 1024 / 1024:.2f} MB", end='\r')

        f.write(footer)

    print(f"\nDone! File {filename} created. Size: {os.path.getsize(filename) / 1024 / 1024:.2f} MB")

if __name__ == "__main__":
    generate_canada_json("canada_large.json", 256)
