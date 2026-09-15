# EV Charging Big Data Project - Development Instructions

## Project Overview

This is a Phase 2 EV charging big-data analysis and prediction project.

The project uses:

- Hadoop / HDFS for distributed data storage
- Apache Spark 3.4.1
- PySpark for data processing
- Spark SQL for ODS/DWD/DWS/ADS analysis
- Spark MLlib for machine learning
- FastAPI for backend APIs
- Static HTML/JavaScript + ECharts for dashboard visualization

The project is developed on Windows using PyCharm/Codex, but the actual
PySpark/Hadoop/Spark execution environment is the teacher-provided Linux VM.

## Execution Environment

The actual big-data environment is the teacher-provided VMware VM.

OS:

- CentOS Linux 7

User:

- hadoop

Password:

- hadoop

Important software locations:

JDK:

```text
/opt/module/jdk1.8.0_144
```

Hadoop:

```text
/opt/module/hadoop-3.3.0
```

Spark:

```text
/opt/module/spark-3.4.1
```

Python:

```text
/bin/python3
```

Python version:

```text
3.10.13
```

pip:

```text
25.1.1
```

Spark version:

```text
3.4.1
```

Do not assume the Windows Python environment has PySpark configured.

Do not install another Hadoop/Spark version unless explicitly requested.

For Spark jobs in the VM, use:

```bash
export PYSPARK_PYTHON=/bin/python3
export PYSPARK_DRIVER_PYTHON=/bin/python3
```

## Shared Project Directory

The project is edited from Windows through VMware shared folders.

The VMware shared-folder name is:

```text
VMShare
```

It maps directly to the Windows project directory:

```text
C:\Users\User\OneDrive\Dokumen\BIT\SEM 5\SHORT SEM\ChargingPlatform
```

Therefore the Linux project root is:

```text
/mnt/hgfs/VMShare
```

Do not append `/ChargingPlatform` to that path. The project is mounted directly
inside `VMShare`.

The project root should contain:

```text
AGENTS.md
bigdata/
client/
dashboard/
data/
db/
ml/
protocol/
server/
requirements.txt
```

Edit files from Windows, then run Spark, PySpark, Hadoop, and Linux tests from
the same mounted directory in the VM. Do not maintain a separate manually
edited copy of the project.

## Local Spark Verification

The teacher-provided VM is the authoritative Spark environment. Windows
Python is not required to have PySpark installed.

Use local file URIs when verifying Spark jobs against the VMware shared folder.
This avoids accidental HDFS access when NameNode is not running.

```bash
cd /mnt/hgfs/VMShare
spark-submit --master 'local[2]' ml/features.py \
  --input-root file:///mnt/hgfs/VMShare/data/raw \
  --output file:///mnt/hgfs/VMShare/data/processed/ml_training_dataset.csv
```

If terminal paste does not preserve multiline commands, run the same command as
a single line:

```bash
spark-submit --master 'local[2]' ml/features.py --input-root file:///mnt/hgfs/VMShare/data/raw --output file:///mnt/hgfs/VMShare/data/processed/ml_training_dataset.csv
```

Do not omit `spark-submit`; otherwise Bash may try to interpret `ml/features.py`
as a shell script.

The current ML feature pipeline uses these six source files:

```text
data/raw/charging_orders.csv
data/raw/stations.csv
data/raw/devices.csv
data/raw/users.csv
data/raw/weather_hourly.csv
data/raw/device_status_log.csv
```

Do not use old `nvv2t.csv` or `nvv2t_md_end.csv` paths when verifying the
current six-table ML feature pipeline.

## Verified Commands

Run the focused tests with Python's built-in unittest if `pytest` is not
available:

```bash
cd /mnt/hgfs/VMShare
python3 -m unittest bigdata.tests.test_charging_pipeline tests.test_ml_features_contract -v
```

The latest verified run completed successfully:

```text
Ran 11 tests
OK
```

After building the ML training dataset, Spark should create:

```text
data/processed/ml_training_dataset.csv/
```

Expected contents include:

```text
part-*.csv
_SUCCESS
```

Inspect the output header with:

```bash
head -n 1 data/processed/ml_training_dataset.csv/part-*.csv
```
