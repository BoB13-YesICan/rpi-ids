import os
import subprocess
import threading
import re
from datetime import datetime
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QTextEdit, QLabel, QPushButton, QWidget, QGridLayout, QDialog, QScrollArea, QMainWindow, QFileDialog, QSizePolicy
from PyQt5.QtCore import QTimer, pyqtSignal, Qt, QMargins, QThread, QObject
from PyQt5.QtChart import QChart, QChartView, QPieSeries
from PyQt5.QtGui import QPainter

# Qt 환경 설정
os.environ["QT_QPA_PLATFORM"] = "xcb" #
class Worker(QObject):
    finished = pyqtSignal()

    def __init__(self, perform_make_and_run, file_name):
        super().__init__()
        self.perform_make_and_run = perform_make_and_run
        self.file_name = file_name

    def run(self):
        try:
            # Call the provided perform_make_and_run method
            self.perform_make_and_run(self.file_name)
        finally:
            self.finished.emit()

class MainApp(QWidget):
    stdout_signal = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.initUI()
        self.stdout_signal.connect(self.update_log)

    def initUI(self):
        self.setWindowTitle("Main Window")
        self.setGeometry(300, 300, 400, 300)

        # 메인 레이아웃
        main_layout = QVBoxLayout()

        # 안내 레이블
        label = QLabel("Welcome to CAN Monitor App")
        label.setStyleSheet("font-size: 18px; font-weight: bold; text-align: center;")
        main_layout.addWidget(label)

        # 버튼 레이아웃
        button_layout = QHBoxLayout()

        # "Select File and Start Monitoring" 버튼
        select_file_button = QPushButton("Start with a new dbc")
        select_file_button.clicked.connect(self.select_file)
        button_layout.addWidget(select_file_button)

        # "Stop Monitoring" 버튼 추가
        start_ids_button = QPushButton("Start with existing ids program")
        start_ids_button.clicked.connect(self.start_ids)
        button_layout.addWidget(start_ids_button)

        # 버튼 레이아웃 추가
        button_widget = QWidget()
        button_widget.setLayout(button_layout)
        main_layout.addWidget(button_widget)

        # 로그 출력 영역
        self.log_output = QTextEdit(self)
        self.log_output.setReadOnly(True)
        main_layout.addWidget(self.log_output)

        # 메인 레이아웃 설정
        self.setLayout(main_layout)

    def start_ids(self):
        # 현재 날짜와 시간을 기반으로 로그 파일 이름 생성
        log_filename = datetime.now().strftime("%Y-%m-%d_%H-%M-%S.log")
        
        # 로그 파일 저장 경로 설정
        log_file_path = os.path.join(os.getcwd(), log_filename)

        # 컴파일된 ./ids 실행
        self.stdout_signal.emit(f"Executing ./ids with log file: {log_file_path}")
        #process = subprocess.Popen(['./ids', log_file_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        c_program_path = "./ids"
        #log_filename = "temp.log"
        self.second_window = CANMonitorApp(c_program_path, log_filename)
        self.second_window.show()

    def update_log(self, message):
        self.log_output.append(message)

    def select_file(self):
        # 파일 다이얼로그를 열어 파일 선택
        options = QFileDialog.Options()
        file_name, _ = QFileDialog.getOpenFileName(
            self, "Select DBC File", "", "DBC Files (*.dbc);;All Files (*)", options=options
        )
        if file_name:
            # QThread 및 Worker 생성
            self.worker_thread = QThread()
            self.worker = Worker(self.perform_make_and_run, file_name)
            self.worker.moveToThread(self.worker_thread)

            # 연결 설정
            self.worker_thread.started.connect(self.worker.run)
            self.worker.finished.connect(self.worker.deleteLater)
            self.worker_thread.finished.connect(self.worker_thread.deleteLater)

            # 쓰레드 시작
            self.worker_thread.start()

    def handle_error(self, error_message):
        print(f"Error: {error_message}")

    def perform_make_and_run(self, dbc_filename):
        # main 로직 실행
        cpp_file = "/home/nojam/다운로드/rpi-ids-feat-GUI/protocol/dbcparsed_dbc.cpp"
        os.makedirs(os.path.dirname(cpp_file), exist_ok=True)

        # DBC 파일 파싱 및 CAN 메시지 목록 생성
        messages = self.parse_dbc_file(dbc_filename)

        # CAN ID 기준으로 메시지를 오름차순 정렬하여 JSON 파일에 기록
        self.write_cpp(messages, cpp_file)

        # DBC 파일에서 CAN ID의 수를 출력
        self.stdout_signal.emit(f"DBC 파일에서 CAN ID 갯수: {len(messages)}")

        # Make 명령어 수행
        self.stdout_signal.emit("Running make...")
        try:
            process = subprocess.Popen(["make"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            for line in iter(process.stdout.readline, ''):
                self.stdout_signal.emit(line.strip())
            process.stdout.close()
            process.wait()
            if process.returncode == 0:
                self.stdout_signal.emit("Make command executed successfully.")
            else:
                self.stdout_signal.emit(f"Make command failed with return code {process.returncode}.")
                return
        except subprocess.CalledProcessError as e:
            self.stdout_signal.emit(f"Make command failed: {e}")
            return

        # 현재 날짜와 시간을 기반으로 로그 파일 이름 생성
        log_filename = datetime.now().strftime("%Y-%m-%d_%H-%M-%S.log")

        # 로그 파일 저장 경로 설정
        log_file_path = os.path.join(os.getcwd(), log_filename)

        # 컴파일된 ./ids 실행
        self.stdout_signal.emit(f"Executing ./ids with log file: {log_file_path}")
        #process = subprocess.Popen(['./ids', log_file_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        c_program_path = "./ids"
        #log_filename = "temp.log"
        self.second_window = CANMonitorApp(c_program_path, log_filename)
        self.second_window.show()

    def update_log(self, message):
        self.log_output.append(message)
        self.log_output.ensureCursorVisible()

    def parse_dbc_file(self, file_path):
        messages = []
        current_message = None

        with open(file_path, 'r') as file:
            for line in file:
                line = line.strip()

                # Ignore specific lines like BU_SG_REL_, SG_MUL_VAL_, and comments
                if line.startswith("BU_SG_REL_") or line.startswith("SG_MUL_VAL_") or line.startswith("CM_ SG_"):
                    continue

                # Match CAN message line
                bo_match = re.match(r"^BO_\s+(\d+)\s+(\w+):\s+(\d+)\s+(\w+)", line)
                if bo_match:
                    can_id = int(bo_match.group(1))
                    message_name = bo_match.group(2)
                    dlc = int(bo_match.group(3))
                    transmitter = bo_match.group(4)

                    # If a message was already in progress, store it
                    if current_message:
                        messages.append(current_message)

                    # Create a new CANMessage object
                    current_message = CANMessage(can_id, message_name, dlc, transmitter)
                    continue

                # Match CAN signal line
                sg_match = re.match(
                    r"^SG_\s+([\w\s]+)\s*:\s+(\d+)\|(\d+)@(\d+)([+-])\s+\(([\d\.E\-]+),\s*([\d\.E\-]+)\)\s+\[([\d\.\-]+)\|([\d\.\-]+)\]\s*\"([^\"]*)\"\s*([\w,]*)",
                    line)
                if sg_match and current_message:
                    signal_name = sg_match.group(1).strip()  # Remove any extra spaces
                    start_bit = int(sg_match.group(2))
                    length = int(sg_match.group(3))
                    byte_order = int(sg_match.group(4))
                    byte_order_sign = sg_match.group(5)  # 부호 (+/-) 캡처
                    factor = float(sg_match.group(6))  # Adjusted group for factor
                    offset = float(sg_match.group(7))  # Adjusted group for offset
                    min_val = float(sg_match.group(8))
                    max_val = float(sg_match.group(9))
                    unit = sg_match.group(10)
                    receiver = sg_match.group(11)

                    # Create a Signal object and add it to the current message
                    signal = Signal(signal_name, start_bit, length, byte_order, byte_order_sign, factor, offset, min_val,
                                    max_val, unit, receiver, min_val, max_val)
                    current_message.add_signal(signal)

                else:
                    # Debug log to identify unparsed signals
                    if "SG_" in line:
                        self.stdout_signal.emit(f"Unparsed signal: {line}")

            # Add the last message to the list
            if current_message:
                messages.append(current_message)

        return messages

    def write_cpp(self, messages, cpp_file):
        # Ensure the directory exists
        directory = os.path.dirname(cpp_file)
        if not os.path.exists(directory):
            try:
                os.makedirs(directory)
                self.stdout_signal.emit(f"Directory created: {directory}")
            except Exception as e:
                self.stdout_signal.emit(f"Failed to create directory {directory}: {e}")
                return

        # Verify if directory is writable
        if not os.access(directory, os.W_OK):
            self.stdout_signal.emit(f"Directory is not writable: {directory}")
            return
        # Sort the messages by CAN ID in ascending order
        sorted_messages = sorted(messages, key=lambda msg: msg.can_id)

        # Prepare the data to be written as C++ code
        cpp_data = "#include <unordered_map>\n#include <string>\n#include <vector>\n#include \"dbcparsed_dbc.h\"\n"

        cpp_data += "std::unordered_map<int, CANMessage> message = {\n"

        for message in sorted_messages:
            message.func_skipable()
            cpp_data += f'    {{0x{message.can_id:x}, {{true, "{message.message_name}", {message.dlc}, "{message.transmitter}", {{\n'

            for signal in message.signals:
                cpp_data += f'        {{"{signal.name}", {signal.start_bit}, {signal.length}, {signal.byte_order}, {1 if signal.byte_order_sign == "-" else 0}, {signal.min}, {signal.max}}},\n'

            cpp_data = cpp_data.rstrip(",\n") + "\n    }}},\n"  # Closing the signal list and message
        cpp_data = cpp_data.rstrip(",\n") + "\n};\n"  # Closing the CANMessages map

        # Write the C++ formatted data to a file
        try:
            with open(cpp_file, 'w') as f:
                f.write(cpp_data)
            self.stdout_signal.emit(f"File successfully written: {cpp_file}")

        except Exception as e:
            self.stdout_signal.emit(f"Failed to write file {cpp_file}: {e}")
        return len(sorted_messages)

class Signal:
    def __init__(self, name, start_bit, length, byte_order, byte_order_sign, factor, offset, min_val, max_val, unit,
                 receiver, min_range, max_range):
        self.name = name
        self.start_bit = start_bit
        self.length = length
        self.byte_order = byte_order
        self.byte_order_sign = byte_order_sign  # 부호 저장
        self.factor = factor
        self.offset = offset
        self.min_val = min_val
        self.max_val = max_val
        self.unit = unit
        self.receiver = receiver
        self.min = min_range
        self.max = max_range
        self.calculate_min_max()  # 계산을 바로 적용

        self.need_to_check = False
        self.is_need_to_check()

    # 신호의 최소/최대 값을 계산하는 함수
    def calculate_min_max(self):
        if self.factor < 0:
            self.min = int(round((self.max_val - self.offset) / self.factor))
            self.max = int(round((self.min_val - self.offset) / self.factor))
        elif self.factor > 0:
            self.min = int(round((self.min_val - self.offset) / self.factor))
            self.max = int(round((self.max_val - self.offset) / self.factor))
        else:
            print(f"ERROR: Factor is zero for signal {self.name}")

    def is_need_to_check(self):
        if self.min != 0 or self.max < ((2 ** self.length) - 1): self.need_to_check = True

class CANMessage:
    def __init__(self, can_id, message_name, dlc, transmitter):
        self.can_id = can_id
        self.skipable = True
        self.message_name = message_name
        self.dlc = dlc
        self.transmitter = transmitter
        self.signals = []

    def add_signal(self, signal):
        self.signals.append(signal)

    def func_skipable(self):
        for signal in self.signals:
            if signal.need_to_check: self.skipable = False




class CANMonitorApp(QWidget):
    stdout_signal = pyqtSignal(str)  # 문자열 데이터를 전달하는 신호

    def __init__(self, c_program_path, log_filename):
        super().__init__()
        self.stdout_signal.connect(self.update_stdout)  # 신호를 슬롯에 연결
        self.c_program_path = c_program_path  # C 프로그램 실행 파일 경로
        self.log_filename = log_filename      # 로그 파일 이름
        self.attack_id_counts = {  # 공격별 카운트를 저장하는 딕셔너리
            "DoS": {},
            "Replay": {},
            "Fuzzing": {},
            "Suspension": {},
            "Masquerade": {},
        }

        self.attack_counts = {  # 공격별 카운트를 저장하는 딕셔너리
            "DoS": 0,
            "Replay": 0,
            "Fuzzing": 0,
            "Suspension": 0,
            "Masquerade": 0,
            "All":0
        }

        # 각 공격 ID 저장
        self.attack_ids = {k: "" for k in self.attack_id_counts.keys()}
        self.total_counts=0
        self.total_attack_counts = 0
        self.initUI()
        self.c_process = None

    def initUI(self):
        self.setWindowTitle("CAN Monitor GUI")
        self.setGeometry(200, 200, 800, 600)

        # 메인 레이아웃
        main_layout = QVBoxLayout()

        # 상단 섹션 (Title)
        self.title_label = QLabel("NORMAL")
        self.title_label.setStyleSheet("font-size: 20px; font-weight: bold; text-align: center;")
        main_layout.addWidget(self.title_label)

        # 중단 섹션 (공격 카운트와 ID 표시)
        middle_layout = QGridLayout()

        # 공격별 카운트와 ID
        self.attack_labels = {}
        for i, attack in enumerate(self.attack_counts.keys()):
            label = QLabel(f"{attack}: 0")
            label.setStyleSheet("background-color: lightgreen; border: 1px solid black; padding: 5px;")
            middle_layout.addWidget(label, i // 2, i % 2)
            self.attack_labels[attack] = label
        self.attack_labels["All"].setStyleSheet("background-color: skyblue; border: 1px solid black; padding: 5px;")
        middle_widget = QWidget()
        middle_widget.setLayout(middle_layout)
        main_layout.addWidget(middle_widget)

        # 원형 그래프 추가
        self.chart_view = self.create_pie_chart()
        main_layout.addWidget(self.chart_view)

        # 중앙 섹션 (로그 출력 영역)
        log_layout = QHBoxLayout()

        # C 프로그램 출력 영역
        self.stdout_text = QTextEdit(self)
        self.stdout_text.setReadOnly(True)
        self.stdout_text.setPlaceholderText("기존 코드에서 C 프로그램 출력하는 곳")
        log_layout.addWidget(self.stdout_text)

        # 로그 파일 출력 영역
        self.log_text = QTextEdit(self)
        self.log_text.setReadOnly(True)
        self.log_text.setPlaceholderText("기존 코드에서 로그 파일 출력하는 곳")
        log_layout.addWidget(self.log_text)

        log_widget = QWidget()
        log_widget.setLayout(log_layout)
        main_layout.addWidget(log_widget)

        # 하단 섹션 (버튼)
        button_layout = QHBoxLayout()

        # "Start" 버튼
        self.start_button = QPushButton("start")
        self.start_button.clicked.connect(self.start_monitoring)
        button_layout.addWidget(self.start_button)

        self.stop_button = QPushButton("stop")
        self.stop_button.clicked.connect(self.stop_monitoring)
        button_layout.addWidget(self.stop_button)

        self.graph_button=QPushButton("Graph")
        self.graph_button.clicked.connect(self.show_detection_detail)
        button_layout.addWidget(self.graph_button)

        button_widget = QWidget()
        button_widget.setLayout(button_layout)
        main_layout.addWidget(button_widget)

        # 메인 레이아웃 설정
        self.setLayout(main_layout)

        # 로그 파일 갱신 타이머
        self.log_timer = QTimer(self)
        self.log_timer.timeout.connect(self.update_log)

    def create_pie_chart(self):
        """
        Create and return a QChartView containing a pie chart.
        """
        # Create a QPieSeries
        series = QPieSeries()
        for attack, count in self.attack_counts.items():
            if attack != "All":
                series.append(f"{attack}: {count}", count)

        # Create a chart and set its title
        chart = QChart()
        chart.addSeries(series)
        chart.setTitle("Attack Count Distribution")
        chart.setMargins(QMargins(10, 10, 10, 10))  # 여백 설정
        chart.setMinimumSize(600, 400)  # 그래프 크기 설정

        chart.legend().setVisible(True)
        chart.legend().setAlignment(Qt.AlignRight)
        # chart.legend().setOrientation(Qt.Vertical)  # 세로 방향으로 나열

        # Create a QChartView to display the chart
        chart_view = QChartView(chart)
        chart_view.setRenderHint(QPainter.Antialiasing)

        # 최소 크기 설정 대신 레이아웃 제약을 설정하거나 크기 조정을 유연하게 변경
        chart_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        # 기본 크기를 설정하여 UI 레이아웃 충돌 방지
        chart_view.resize(700, 500)

        # 배경 및 기타 속성 설정 (옵션)
        chart.setBackgroundBrush(Qt.white)  # 배경을 흰색으로 설정 (필요 시)
        chart.setMargins(QMargins(10, 10, 10, 10))  # 적절한 여백 설정

        return chart_view
    
    def update_pie_chart(self):
        """
        Update the pie chart based on the current attack counts.
        """
        # Get the current chart from the chart view
        chart = self.chart_view.chart()

        # Clear the old series
        chart.removeAllSeries()

        # Create a new QPieSeries
        series = QPieSeries()
        for attack, count in self.attack_counts.items():
            if attack != "All":
                series.append(f"{attack}: {count}", count)

        # Add the new series to the chart
        chart.addSeries(series)
        # Ensure legend remains visible
        chart.legend().setVisible(True)
        chart.legend().setAlignment(Qt.AlignRight)  # 범례를 오른쪽에 정렬

    def show_detection_detail(self):
        # 새 창 생성
        dialog = QDialog(self)
        dialog.setWindowTitle("Detection Details")
        dialog.setGeometry(300, 300, 800, 600)

        # 메인 레이아웃 설정
        main_layout = QVBoxLayout()

        # 공격별 상세 정보 표시
        for attack_type, attack_data in self.attack_id_counts.items():
            if not attack_data:  # 데이터가 없는 경우 넘어감
                continue

            # 제목 표시
            title_label = QLabel(f"Attack Type: {attack_type}")
            title_label.setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px 0;")
            main_layout.addWidget(title_label)

            # 원형 그래프 생성 및 추가
            chart_view = self.create_attack_pie_chart(attack_type, attack_data)
            main_layout.addWidget(chart_view)

        # 스크롤 영역 설정 (그래프가 많아질 경우 대비)
        scroll_area = QScrollArea()
        scroll_widget = QWidget()
        scroll_widget.setLayout(main_layout)
        scroll_area.setWidget(scroll_widget)
        scroll_area.setWidgetResizable(True)

        # 다이얼로그 레이아웃 설정
        dialog_layout = QVBoxLayout()
        dialog_layout.addWidget(scroll_area)
        dialog.setLayout(dialog_layout)

        # 다이얼로그 표시
        dialog.exec_()

    def create_attack_pie_chart(self, attack_type, attack_data):
        """
        공격 유형별 데이터를 기반으로 원형 그래프 생성
        """
        series = QPieSeries()
        for attack_id, count in attack_data.items():
            series.append(f"{attack_id}: {count}", count)

        # 차트 설정
        chart = QChart()
        chart.addSeries(series)
        chart.setTitle(f"{attack_type} Attack Details")
        chart.setMargins(QMargins(10, 10, 10, 10))
        chart.legend().setVisible(True)
        chart.legend().setAlignment(Qt.AlignRight)

        # 차트 뷰 생성
        chart_view = QChartView(chart)
        chart_view.setRenderHint(QPainter.Antialiasing)

        # 최소 크기 설정 대신 레이아웃 제약을 설정하거나 크기 조정을 유연하게 변경
        chart_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

        # 기본 크기를 설정하여 UI 레이아웃 충돌 방지
        chart_view.resize(600, 400)
        

        return chart_view
    # 나머지 함수는 기존 코드와 동일
    def start_monitoring(self):
        if not os.path.exists(self.c_program_path):#C 프로그램 파일이 존재하는지 확인.
            self.stdout_text.append(f"Error: {self.c_program_path} not found!")
            self.scroll_to_bottom(self.stdout_text)
            return

        try:
            self.c_process = subprocess.Popen(#C 프로그램을 실행하고 stdout과 stderr를 읽음.
                [self.c_program_path, self.log_filename],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            self.stdout_thread = threading.Thread(target=self.read_stdout)#별도 스레드에서 stdout을 읽는 read_stdout 실행.
            self.stdout_thread.daemon = True
            self.stdout_thread.start()
            # self.log_timer.start(100)  # 1초(1000ms) 간격으로 로그 업데이트.
            # QTimer.singleShot(0, self.log_timer.start(100))  # 메인 스레드에서 타이머 시작
            QTimer.singleShot(0, lambda: self.log_timer.start(1000))  # 1초 간격
        except Exception as e:
            self.stdout_text.append(f"Error starting C program: {e}")
            self.scroll_to_bottom(self.stdout_text)

    def stop_monitoring(self):
        if self.c_process:
            self.c_process.terminate()  # C 프로그램 종료
            self.c_process = None
        self.log_timer.stop()  # 로그 파일 갱신 중지

    def read_stdout(self):
        # C 프로그램 stdout을 읽어 GUI에 출력
        while self.c_process and self.c_process.poll() is None:
            line = self.c_process.stdout.readline()
            if line:
                self.stdout_signal.emit(line.strip())  # 메인 스레드로 전달

    def update_stdout(self, text):
        # 메인 스레드에서 GUI 업데이트
        self.stdout_text.append(text)
        self.scroll_to_bottom(self.stdout_text)
        self.parse_attack(text)  # 공격 분석

    def parse_attack(self, line):
        
        """
        표준 출력에서 공격 유형을 파싱하고, 카운트를 업데이트
        """
        attack_match = re.search(r"\[(\w+)\] \[(\d+)\] \[(\w+)\] (.+)", line)

        if attack_match:
            attack_type = attack_match.group(1)  # 예: DoS
            attack_id = attack_match.group(2)   # 예: 000
            if attack_type not in self.attack_counts:
                print(attack_type+"is not in self.attack_counts")
                return 
            if attack_type not in self.attack_id_counts:
                print("Not attack_type "+ attack_type)
                return
            if attack_id not in self.attack_id_counts[attack_type]:
                print("New Detection ID "+attack_id)
                self.attack_id_counts[attack_type][attack_id] = 0
            self.attack_id_counts[attack_type][attack_id] += 1  # count가 없으면 증가
            self.attack_counts[attack_type]+=1
            self.attack_counts["All"]+=1
            self.total_attack_counts+=1
            
            self.update_attack_labels(attack_type)
        else:print("Not parse attack info")
    

        # for attack in self.attack_counts.keys():
        #     if f"[{attack}]" in line:  # 출력에 공격 유형이 포함된 경우
        #         self.attack_counts[attack] += 1
        #         self.attack_counts["All"] += 1
        #         self.update_attack_labels(attack)
        #         break

        if "Malicious packet" in line:
            self.total_attack_counts+=1

    def update_attack_labels(self, attack):
        # 카운트 업데이트
        self.attack_labels[attack].setText(f"{attack}: {self.attack_counts[attack]}")
        self.attack_labels["All"].setText(f"{"All"}: {self.attack_counts["All"]}")
        # print("label update "+attack)
        # 기존 배경색 저장
        original_title_style="font-size: 20px; font-weight: bold; text-align: center;"
        original_title="NORMAL"
        original_style = "background-color: lightgreen; border: 1px solid black; padding: 5px;"
        # 빨간색으로 변경
        self.attack_labels[attack].setStyleSheet("background-color: red; border: 1px solid black; padding: 5px;")
        self.title_label.setStyleSheet("color: red; font-size: 20px; font-weight: bold; text-align: center;")
        self.title_label.setText("WARNING!!!")

        # 0.5초 후 스타일 복원
        def restore_styles():
            self.attack_labels[attack].setStyleSheet(original_style)
            self.title_label.setStyleSheet(original_title_style)
            self.title_label.setText(original_title)
        # 0.5초 후 원래 색상으로 복원
        QTimer.singleShot(500, restore_styles)
        # Update the pie chart
        self.update_pie_chart()

    def update_log(self):
        # 로그 파일 내용을 읽어와 GUI에 표시
        try:
            with open(self.log_filename, "r") as file:
                self.log_text.setPlainText(file.read())
                self.scroll_to_bottom(self.log_text)  # 로그 파일 갱신 시 스크롤 아래로 이동
        except FileNotFoundError:
            self.log_text.setPlainText("Log file not found!")
            self.scroll_to_bottom(self.log_text)

    def scroll_to_bottom(self, text_edit):
        """
        QTextEdit의 스크롤을 맨 아래로 이동
        """
        cursor = text_edit.textCursor()
        cursor.movePosition(cursor.End)
        text_edit.setTextCursor(cursor)
        text_edit.ensureCursorVisible()

    def closeEvent(self, event):
        self.stop_monitoring()
        event.accept()

if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    main_window = MainApp()
    main_window.show()
    sys.exit(app.exec_())