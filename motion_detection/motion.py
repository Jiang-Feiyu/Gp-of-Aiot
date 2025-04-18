import os
import numpy as np
import csv

def read_csi_data_md(inpt):
    amplitude = [[] for _ in range(117)]
    with open(inpt, 'r', encoding='utf-8') as file:
        reader = csv.DictReader(file)
        for row in reader:
            data_str = row.get('data')
            if data_str:
                data_list = [int(num) for num in data_str.strip('[]').split(',') if num.strip()]
                for i in range(117):
                    imaginary = data_list[2 * i]
                    real = data_list[2 * i + 1]
                    amplitude_value = np.sqrt(imaginary ** 2 + real ** 2)
                    amplitude[i].append(amplitude_value)
    return amplitude

if __name__ == '__main__':
    path = './benchmark/motion_detection/test/'
    csv_files = [f for f in os.listdir(path) if f.endswith('.csv')]
    for csv_file in csv_files:
        inpt = os.path.join(path, csv_file)
        amplitude = read_csi_data_md(inpt)
        variance = np.var(amplitude[115])
        print(csv_file, variance)
