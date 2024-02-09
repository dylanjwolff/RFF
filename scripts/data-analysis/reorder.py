import polars as pl 
import seaborn as sns
import matplotlib.pyplot as plt

ours = "power-coe-always-rand-schedfuzz"
figure_tools = [ours, "pct-no-rff-always-rand-schedfuzz"]

df = pl.read_csv("full-data.csv")
df = df.rename({"": "tool", "_duplicated_0" : "benchmark", "_duplicated_1": "trial_num_1"})
df = df.drop("trial_num_1")
df = df.with_columns(pl.col(
    ["buggy_scheds",
     "scheds_to_bug",
     "total_scheds"]).cast(pl.Int64))

tool_mapping = {
    "pos-only-schedfuzz": "Partial Order Sampling",
    ours: "RAIDFuzzer",
    "no-rff-no-afl-cov-always-rand-schedfuzz": "RAIDFuzzer (−greybox feedback)",
    "qlearning-schedfuzz": "QLearning RF",
    "pct-no-rff-always-rand-schedfuzz": "PCT3",
    "PERIOD": "PERIOD"
}

figure_tools = list(map(tool_mapping.get, figure_tools))
df = df.with_columns(pl.col("tool").map_dict(tool_mapping, default=pl.first()))

df = df.filter(pl.col("benchmark").is_in(
    [
        "CS/reorder_3",
        "CS/reorder_4",
        "CS/reorder_5",
        "CS/reorder_10",
        "CS/reorder_20",
        "CS/reorder_50",
        "CS/reorder_100",
    ]
))

df = df.filter(pl.col("tool").is_in( figure_tools ))

df = df.with_columns(pl.col("benchmark") \
        .apply(lambda s: int(s.split("reorder_")[-1])) \
        .alias("Threads"))

print(df)

pdf = df.to_pandas()


sns.lmplot(pdf, x="Threads", y="scheds_to_bug", hue="tool")
plt.show()
