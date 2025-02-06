import numpy as np
from tflite_runtime.interpreter import Interpreter
import time
import os
import sys

# TFLite 모델 로드 및 예측 함수
def load_and_predict_tflite_model(model_path, input_data):
    interpreter = Interpreter(model_path=model_path)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    interpreter.set_tensor(input_details[0]['index'], input_data)
    interpreter.invoke()

    output_data = interpreter.get_tensor(output_details[0]['index'])
    return output_data

# 침입 탐지 함수
def detect_intrusion(model_path, log_filename, output_log_filename, payload_size=8, timeout=3):
    interpreter = Interpreter(model_path=model_path)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    last_modification_time = 0
    last_position = 0  # Track the last read position
    intrusion_count = 0  # Intrusion count tracker
    last_line = None  # Track the last line read from the log

    while not os.path.exists(log_filename):  # Wait for the log file to be created
        #print(f"Waiting for log file: {log_filename}")
        time.sleep(1)

    last_file_update_time = time.time()  # Track the last update time of the file

    while True:
        try:
            if os.path.exists(log_filename):
                current_modification_time = os.path.getmtime(log_filename)

                # 파일이 업데이트된 경우에만 진행
                if current_modification_time > last_modification_time:
                    last_modification_time = current_modification_time
                    last_file_update_time = time.time()  # Reset the last file update time
                    #print("Processing new log entries...")

                    with open(log_filename, 'r') as log_file, open(output_log_filename, 'a') as output_log:
                        log_file.seek(last_position)
                        lines = log_file.readlines()  # Read new lines only
                        last_position = log_file.tell()

                        for line in lines:
                            #print(f"Processing line: {line.strip()}")
                            parts = line.strip().split()
                            if parts[-1] == '0':
                                try:
                                    # Parse CAN ID and payload
                                    can_id_hex = parts[2].split('#')[0]
                                    payload = parts[2].split('#')[1]
                                    label = int(parts[-1])  # Extract label (last part)

                                    can_id = int(can_id_hex, 16)
                                    payload_bytes = [int(payload[i:i+2], 16) for i in range(0, len(payload), 2)]

                                    # Ensure payload has 8 bytes
                                    if len(payload_bytes) < payload_size:
                                        payload_bytes += [0] * (payload_size - len(payload_bytes))
                                    else:
                                        payload_bytes = payload_bytes[:payload_size]

                                    # Construct input features
                                    input_features = np.array([[can_id] + payload_bytes], dtype=np.float32)

                                    # Perform model prediction
                                    prediction = load_and_predict_tflite_model(model_path, input_features)
                                    is_intrusion = prediction[0] > 0.5  # Example threshold

                                    # Update the line with intrusion detection result
                                    if is_intrusion:
                                        intrusion_count += 1
                                        parts[-1] = "1"  # Update label to 1 for intrusion
                                    else:
                                        parts[-1] = "0"  # Update label to 0 for no intrusion

                                    # Write the updated line to the output log
                                    output_log.write(" ".join(parts) + "\n")
                                except Exception as e:
                                    print(f"Error processing line: {line.strip()}")
                                    print(f"Exception: {e}")
                            else:
                                parts[-1] = "1"
                                output_log.write(" ".join(parts) + "\n")


                    # Update the last processed line to ensure we're checking the most recent line
                    last_line = lines[-1] if lines else None
                else:
                    # Check if 3 seconds have passed since the last file modification
                    if time.time() - last_file_update_time > timeout:
                        #print("No updates in the log file within the last 3 seconds. Stopping detection.")
                        break
            else:
                time.sleep(1)  # Polling interval
        except KeyboardInterrupt:
            #print("Stopping intrusion detection.")
            break

# 주요 코드 실행
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python detect_intrusion.py <log_file_path> <output_log_file_path>")
        sys.exit(1)

    tflite_model_path = "xgboost_model.tflite"
    log_file_path = sys.argv[1]
    output_log_file_path = sys.argv[2]
    feature_count = 10  # Update this with the number of features used in the XGBoost model
    detect_intrusion(tflite_model_path, log_file_path, output_log_file_path, feature_count)
