"""Run the Task #61/#62 Spark ETL against HDFS or local Spark paths."""

import argparse

from bigdata.etl.spark_pipeline import load_charging_orders_spark, load_stations_spark


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--master", default=None, help="Spark master, e.g. local[*]")
    parser.add_argument("--orders-input", default="hdfs:///charging/raw/nvv2t.csv")
    parser.add_argument(
        "--stations-input", default="hdfs:///charging/raw/nvv2t_md_end.csv"
    )
    parser.add_argument(
        "--output-root", default="hdfs:///charging/processed", help="Spark output directory"
    )
    args = parser.parse_args()

    load_charging_orders_spark(
        args.orders_input,
        f"{args.output_root}/charging_orders",
        args.master,
    )
    load_stations_spark(
        args.stations_input,
        f"{args.output_root}/charging_stations",
        args.master,
    )


if __name__ == "__main__":
    main()