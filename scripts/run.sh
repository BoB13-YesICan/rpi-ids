#!/bin/bash

# 1. Python 파일 실행 여부 확인
read -p "DBC 파일을 새롭게 적용하시겠습니까? (y/n): " apply_dbc
if [ "$apply_dbc" = "y" ]; then
    python3 cpp_parser.py
    if [ $? -ne 0 ]; then
        echo "DBC 파일 적용 실패!"
        exit 1
    fi

    echo "Running make..."
    make
    if [ $? -ne 0 ]; then
        echo "Make command failed!"
        exit 1
    fi

else	
    # DBC 적용 안 할 경우 폴더 체크 및 삭제
    target_folder="../protocol/dbcparsed_dbc.cpp"
    if [ -e "$target_folder" ]; then
        echo "$target_folder 폴더가 존재합니다. 삭제 중..."
        rm -rf "$target_folder"
        if [ $? -eq 0 ]; then
            echo "$target_folder 폴더가 삭제되었습니다."
        else
            echo "$target_folder 폴더 삭제 실패!"
            exit 1
        fi
    else
        echo "$target_folder 폴더가 존재하지 않습니다. 넘어갑니다."
    fi
fi

read -p "컴파일을 하시겠습니까?(최초 1회 필수) (y/n): " replay_compile
if [ "$replay_compile" = "y" ]; then
    make
    if [ $? -ne 0 ]; then
        echo "Make command failed!"
        exit 1
    fi
fi

# 2. AI와 룰셋 통합 IDS 실행 여부 확인
read -p "AI와 룰셋 통합 IDS로 실행하시겠습니까? (y/n): " use_ai

# 3. ./ids 실행
log_filename=$(date +"../log/%Y-%m-%d_%H-%M-%S.log")

if [ "$use_ai" = "n" ]; then
    echo "Executing ./ids with log file: $log_filename"
    echo "Clearing Terminal to Start IDS CLI"
    sleep 3
    clear
    ./ids "$log_filename"
    if [ $? -ne 0 ]; then
        echo "./ids execution failed!"
        exit 1
    fi
else
    # AI 통합 실행
    echo "Clearing Terminal to Start IDS CLI"
    sleep 3
    clear

    echo "AI 통합 IDS 실행 준비 중..."
    tflite_model_path="xgboost_model.tflite"
    output_log_file_path=$(date +"../log/%Y-%m-%d_%H-%M-%S_after_AI.log")

    # AI Python 코드 실행
    python3 model_xgboost.py "$log_filename" "$output_log_file_path" &
    model_pid=$!

    echo "Executing ./ids with log file: $log_filename"
    ./ids "$log_filename"
    ids_pid=$!

    # wait for model process to finish
    wait $model_pid

    # If the model process exits, terminate the ./ids process
    echo "AI model process completed. Terminating ./ids process..."
    kill $ids_pid
    
    if [ $? -ne 0 ]; then
        echo "./ids execution failed!"
        kill $model_pid
        exit 1
    fi
fi

echo "All steps completed successfully."

