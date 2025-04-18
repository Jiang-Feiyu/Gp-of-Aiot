#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_LINE_LENGTH 10240
#define MAX_FIELD_LENGTH 1024
#define FILTER_ORDER 4
#define LOW_CUT 0.15
#define HIGH_CUT 0.5
int row_count = 0;
int **data_array;
char **timestamp_array, **check_time;
double *result_array;
double *amplitudes, *filters, bpm[114] = {0};
int num_peaks, num_valleys;
int *peaks;
int *valleys;
int start, end, wave_length, count;
double fs, breath_seconds;

int find_column_index(const char *header_line, const char *column_name) {
    char *token;
    char line_copy[MAX_LINE_LENGTH];
    strcpy(line_copy, header_line);

    int index = 0;
    token = strtok(line_copy, ",");
    while (token != NULL) {
    	char *newline_pos = strchr(token, '\n');
  		if (newline_pos != NULL) {
  			*newline_pos = '\0';
		}
        if (strcmp(token, column_name) == 0) {
            return index;
        }
        token = strtok(NULL, ",");
        index++;
    }
    return -1;
}
const char *removeFirstChar(const char *str) {
    if (str == NULL || str[0] == '\0') {
        return NULL;
    }
    int len = strlen(str);
    char *newStr = (char *)malloc((len) * sizeof(char));
    if (newStr == NULL) {
        return NULL;
    }
    strcpy(newStr, str + 1);
    return newStr;
}
int split_string(const char *data, int *result) {
    char data_copy[strlen(data) + 1];
    strcpy(data_copy, data);

    if (data_copy[0] == '[' && data_copy[strlen(data_copy) - 1] == ']') {
        data_copy[strlen(data_copy) - 1] = '\0';
        memmove(data_copy, data_copy + 1, strlen(data_copy));
    }

    char *token = strtok(data_copy, ",");
    int index = 0;
    while (token != NULL && index < 500) {
        result[index] = atoi(token);
        token = strtok(NULL, ",");
        index++;
    }
    return index;
}
void read_csv(const char *filename, const char *data_column, const char *timestamp_column) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Can not open the file.");
        return;
    }

    char line[MAX_LINE_LENGTH];
    if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
        perror("Failed to read file header.");
        fclose(file);
        return;
    }

    int data_index = find_column_index(line, data_column);
    int timestamp_index = find_column_index(line, timestamp_column);

    if (data_index == -1 || timestamp_index == -1) {
        printf("The specified column was not found.\n");
        fclose(file);
        return;
    }

    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
        row_count++;
    }
    rewind(file);
    fgets(line, MAX_LINE_LENGTH, file);

    data_array = (int **)malloc(row_count * sizeof(int *));
    if (data_array == NULL) {
        perror("Memory allocation failed.");
        fclose(file);
        return;
    }

    timestamp_array = (char **)malloc(row_count * sizeof(char *));
    int i, j; 
    if (timestamp_array == NULL) {
        perror("Memory allocation failed.");
        for (i = 0; i < row_count; i++) {
            free(data_array[i]);
        }
        free(data_array);
        fclose(file);
        return;
    }
    int *data_sizes = (int *)malloc(row_count * sizeof(int));

    int current_row = 0;
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
        char *token;
        int index = 0;
        char data[MAX_FIELD_LENGTH];
        char timestamp[MAX_FIELD_LENGTH];
        //printf("line:%s\n",line);
        token = strtok(line, "\"");
        data_index = 1;
        timestamp_index = 2;

        while (token != NULL) {
            if (index == data_index) {
                strcpy(data, token);
            } else if (index == timestamp_index) {
                strcpy(timestamp, token);
            }
            token = strtok(NULL, "\"");
            index++;
        }
        data[strcspn(data, "\n")] = '\0';
        timestamp[strcspn(timestamp, "\n")] = '\0';

        data_array[current_row] = (int *) (int *) malloc((strlen(data) + 1) * sizeof(int));
        data_sizes[current_row]=split_string(data,data_array[current_row]);        //printf("data is:%s\n",data);

        timestamp_array[current_row] = (char *)malloc((strlen(timestamp) + 1) * sizeof(char));
        strcpy(timestamp_array[current_row], removeFirstChar(timestamp));

        current_row++;
    }

    fclose(file);
}



// 从日期字符串中提取时分秒部分
char* extract_time_part(const char *date_str ) {
    if (date_str == NULL) {
        fprintf(stderr, "Input date string is NULL\n");
        return NULL;
    }
    char *time_part = (char *)malloc(9 * sizeof(char));
    if (time_part == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }
    sscanf(date_str, "%*d-%*d-%*d %8s", time_part);
    return time_part;
}

// 判断两个时分秒字符串是否相同
int is_time_duplicate(const char *time1, const char *time2) {
    return strcmp(time1, time2) == 0;
}

int read_result(const char *filename, const char *data_column) {
    FILE *file = fopen(filename, "r");
    char new_line[MAX_LINE_LENGTH];
    result_array = (double *)malloc(row_count * sizeof(double));
    if (file == NULL) {
        perror("NO");
        return 0;
    }
    if (result_array == NULL) {
        perror("Memory allocation failed for result_array");
        fclose(file);
        return 0;
    }

    // 为 check_time 分配内存，用于存储指向每行 timestamp 的指针
    check_time = (char **)malloc(row_count * sizeof(char *));
    if (check_time == NULL) {
        perror("Memory allocation failed for check_time");
        fclose(file);
        free(result_array);
        return 0;
    }

    int i = 0, j;
    while (fgets(new_line, MAX_LINE_LENGTH, file) != NULL && i < row_count) {
        char *token = strtok(new_line, ",");
        if (token != NULL) {
            double tmp_double = 0.0;
            sscanf(token, "%lf", &tmp_double);

            token = strtok(NULL, ",");
            if (token != NULL) {
                size_t token_len = strlen(token);
                char *current_time = extract_time_part(token);
                if (current_time == NULL) {
                    perror("Failed to extract time part");
                    fclose(file);
                    free(result_array);
                    for (j = 0; j < i; j++) {
                        free(check_time[j]);
                    }
                    free(check_time);
                    return 0;
                }

                int should_store = 1;
                if (i > 0) {
                    char *prev_time = extract_time_part(check_time[i - 1]);
                    if (prev_time != NULL) {
                        if (is_time_duplicate(prev_time, current_time)) {
                            should_store = 0;
                        }
                        free(prev_time);
                    }
                }

                if (should_store) {
                    check_time[i] = (char *)malloc((token_len + 1) * sizeof(char));
                    if (check_time[i] == NULL) {
                        perror("Memory allocation failed for check_time[i]");
                        fclose(file);
                        free(result_array);
                        for (j = 0; j < i; j++) {
                            free(check_time[j]);
                        }
                        free(check_time);
                        free(current_time);
                        return 0;
                    }
                    strcpy(check_time[i], token);
                    result_array[i] = tmp_double;
                    i++;
                }
                free(current_time);
            }
        }
    }

    fclose(file);

    // 释放内存
//    for (j = 0; j < i; j++) {
//        free(check_time[j]);
//    }
//    free(check_time);
//    free(result_array);

    return i;
}

float calculate_amplitude(float imaginary, float real) {
	return sqrt(imaginary * imaginary + real * real);
}

void find_peaks_valleys(double *amplitudes, int num_data, int *peaks, int *num_peaks, int *valleys, int *num_valleys) {
    *num_peaks = 0;
    *num_valleys = 0;
    int i;

    for (i = 1; i < num_data - 1; i++) {
        if (amplitudes[i] > amplitudes[i - 1] && amplitudes[i] > amplitudes[i + 1]) {
            peaks[(*num_peaks)++] = i;
        }
        if (amplitudes[i] < amplitudes[i - 1] && amplitudes[i] < amplitudes[i + 1]) {
            valleys[(*num_valleys)++] = i;
        }
    }
}

int extract_seconds(const char *timestamp) {
    int seconds;
    sscanf(timestamp, "%*d-%*d-%*d %*d:%*d:%d", &seconds);
    return seconds;
}

void increment_time(char *target_time) {
    int year, month, day, hour, minute, second;
    // 解析时间字符串
    sscanf(target_time, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    second++;
    if (second == 60) {
        second = 0;
        minute++;
        if (minute == 60) {
            minute = 0;
            hour++;
            if (hour == 24) {
                hour = 0;
                // 这里暂不处理日期进位（如跨月、跨年），若需要可进一步完善
            }
        }
    }
    // 重新格式化时间字符串
    sprintf(target_time, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
}

int compare_time(const char *time1, const char *time2) {
	return strcmp(time1, time2);
}

double prewarp(double w, double fs) {
    return (2 * fs / M_PI) * tan(M_PI * w / fs);
}

void butter_bandpass(double fs, double lowcut, double highcut, double *b, double *a) {
    double nyquist = fs / 2.0;
    double low = lowcut / nyquist;
    double high = highcut / nyquist;
    
    // 计算 Butterworth 带通滤波器系数
    double center_freq = sqrt(low * high);
    double bandwidth = high - low;
    int i;
    
    // 归一化频率
    double Wn = bandwidth;
    double alpha = sin(Wn * M_PI / 2.0) / cos(Wn * M_PI / 2.0);
    
    // 计算 b 和 a 系数
    b[0] = alpha;
    b[1] = 0;
    b[2] = -alpha;
    
    a[0] = 1 + alpha;
    a[1] = -2 * cos(M_PI * center_freq);
    a[2] = 1 - alpha;
    
    // 归一化系数
    double gain = (1 + alpha) / 2.0;
    for (i = 0; i < 5; i++) {
        b[i] /= gain;
        a[i] /= gain;
    }
}

void iir_filter(double *x, double* b, double* a, double* y, int len) {
	int i, j;
    for (i = 0; i < len; i++) {
        y[i] = 0.0;
        for (j = 0; j <= FILTER_ORDER; j++) {
            if (i - j >= 0) y[i] += b[j] * x[i - j];
            if (j > 0 && i - j >= 0) y[i] -= a[j] * y[i - j];
        }
    }
}

int compare(const void *a, const void *b) {
    double diff = (*(double *)a - *(double *)b);
    if (diff < 0) return -1;
    else if (diff > 0) return 1;
    else return 0;
}

int remove_outliers(double *array, int size, double *filtered_array, double threshold_low, double threshold_high) {
    int count = 0, i;
    for (i = 0; i < size; i++) {
        if (array[i] >= threshold_low && array[i] <= threshold_high) {
            filtered_array[count++] = array[i];
        }
    }
    return count;
}

double calculate_mean(double *array, int size) {
    double sum = 0.0;
    int i;
    for (i = 0; i < size; i++) {
        sum += array[i];
    }
    return sum / size;
}

double calculate_median(double *array, int size) {
    qsort(array, size, sizeof(double), compare);  // 排序
    if (size % 2 == 0) {
        // 偶数个元素，返回中间两个数的平均值
        return (array[size / 2 - 1] + array[size / 2]) / 2.0;
    } else {
        // 奇数个元素，返回中间值
        return array[size / 2];
    }
}

double calculate_mode(double *array, int size) {
    qsort(array, size, sizeof(double), compare);  // 排序
    double mode = array[0];
    int i;
    int max_count = 1, current_count = 1;

    for (i = 1; i < size; i++) {
        if (array[i] == array[i - 1]) {
            current_count++;
        } else {
            if (current_count > max_count) {
                max_count = current_count;
                mode = array[i - 1];
            }
            current_count = 1;
        }
    }
    // 检查最后一组
    if (current_count > max_count) {
        mode = array[size - 1];
    }

    return mode;
}

int main() {
    const char *filename = "CSI20250227_193124.csv";
    const char *data_column = "data";
    const char *timestamp_column = "timestamp";
    const char *resultname = "gt_20250227_193124.csv";
    int i, j = 0, k = 0, total;
	read_csv(filename, data_column, timestamp_column);
    total = read_result(resultname, data_column);
    for (i = 1; i < total; i++) {
    	for (j = 0; j < 117; j++) {
    		if (j == 57 || j == 58 || j == 59) {
    			continue;
			}
			int idx = 0;
			char target_time[20], start_time[20];
			strcpy(target_time, check_time[i]);
			strcpy(start_time, target_time);
			increment_time(target_time);
			if (compare_time(target_time, timestamp_array[row_count - 1]) > 0) {
				return 0;
			}
			int hours, minutes, seconds;
        	sscanf(start_time + 11, "%d:%d:%d", &hours, &minutes, &seconds);
        	seconds -= 14;
        	if (seconds < 0) {
            	seconds += 60;
            	minutes--;
            	if (minutes < 0) {
                	minutes += 60;
                	hours--;
            	}
        	}
        	sprintf(start_time + 11, "%02d:%02d:%02d", hours, minutes, seconds);
        	printf("%s %s %s\n", start_time, target_time, check_time[i]);
        	
    		int num = 0, last_index = -1;
    		for (k = 0; k < row_count; k++) {
    			if (compare_time(timestamp_array[k], start_time) >= 0 && compare_time(timestamp_array[k], target_time) <= 0) {
    				num++;
    				last_index = k;
				}
			}
			if (last_index != -1) {
        		printf("第一个数据的时间是: %s\n", start_time);
        		printf("15 秒后的最后一个数据的位置是: %d\n", last_index);
        		printf("这期间的数据数量是: %d\n", num);
    		} else {
        		printf("未找到符合条件的数据。\n");
    		}
    		fs = (num - 1) / 15.0;
    		printf("fs = %.8f\n", fs);
    		amplitudes = (double *)malloc(2 * row_count * sizeof(double));
    		peaks = (int *)malloc(row_count * sizeof(int));
    		valleys = (int *)malloc(row_count * sizeof(int));
    		filters = (double *)malloc(2 * row_count * sizeof(double));
    		double b[FILTER_ORDER + 1] = {0.00011905, 0.0, -0.00023809, 0.0, 0.00011905};
    		double a[FILTER_ORDER + 1] = {1.0, -3.96831469, 5.90601698, -3.90708066, 0.96937846};
    		for (k = 0; k < row_count; k++) {
    			amplitudes[idx++] = data_array[k][2 * j];
    			amplitudes[idx++] = data_array[k][2 * j + 1];
			}
	
			iir_filter(amplitudes, b, a, filters, idx);
		
			find_peaks_valleys(filters, num - 1, peaks, &num_peaks, valleys, &num_valleys);
    		start = (peaks[0] < valleys[0]) ? peaks[0] : valleys[0];
    		end = (peaks[num_peaks - 1] > valleys[num_valleys - 1]) ? peaks[num_peaks - 1] : valleys[num_valleys - 1];
    	
    		wave_length = end - start;
    		count = num_peaks + num_valleys - 1;
    		breath_seconds = wave_length / count / fs;
    		bpm[j] = 60.0 * breath_seconds;
    		printf("%lf\n", bpm[j]);
    	
    		free(amplitudes);
    		free(peaks);
    		free(valleys);
    		free(filters);
    		
		}
		double filtered_bpm[114];
    	int filtered_size = remove_outliers(bpm, 114, filtered_bpm, 12.0, 25.0);
    
    	double mode = calculate_mode(filtered_bpm, filtered_size);
    	double median = calculate_median(filtered_bpm, filtered_size);
    	double average = calculate_mean(filtered_bpm, filtered_size);
    
    	printf("众数: %f\n", mode);
    	printf("中位数: %f\n", median);
    	printf("平均数: %f\n", average);
    
    	double ae = 0, mae;
    	printf("%d\n", total); 
    	for (j = 0; j < total; j++) {
    		ae += abs(average - result_array[j]);
		}
		mae = ae / total;
		printf("%s mae: %lf\n", check_time[i], mae);
	}
    return 0;
}
