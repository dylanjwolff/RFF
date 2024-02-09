import pandas as pd
import polars as pl
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from lifelines import KaplanMeierFitter
from lifelines.utils import restricted_mean_survival_time
from lifelines.statistics import logrank_test
from scipy.stats import mannwhitneyu
import itertools
import math
import os

paper = True
ours = "power-coe-always-rand-schedfuzz"

figure_tools = [
    "pct-no-rff-always-rand-schedfuzz",
    "PERIOD",
    ours,
    "pos-only-schedfuzz",
    "qlearning-schedfuzz",
]

table_tools = [
    "GenMC",
]

# figure_tools = [
#     ours,
#     "qlearning-schedfuzz",
# ]


os.makedirs("assets", exist_ok=True)

df = pl.read_csv("scripts/data-analysis/full-data.csv", infer_schema_length=1000)
df = df.rename({"": "tool", "_duplicated_0" : "benchmark", "_duplicated_1": "trial_num_1"})
df = df.drop("trial_num_1")
df = df.with_columns(pl.col(
    ["buggy_scheds",
     "scheds_to_bug",
     "total_scheds"]).cast(pl.Int64))

period_df = pl.read_csv("scripts/data-analysis/finalResults.csv")
period_df = period_df.rename({
    "name": "benchmark",
    "trail" : "trial_num",
    "numToFirstBug": "scheds_to_bug",
    "numBuggy": "buggy_scheds",
    "totalNum": "total_scheds",
})
period_df = period_df.with_columns(pl.lit("PERIOD").alias("tool"))

period_df = period_df.with_columns(
    pl.col("benchmark").map_elements(lambda x: x.replace("/workdir/PERIOD/evaluation/", "")))
period_df = period_df.with_columns(
    pl.col("scheds_to_bug").map_elements(lambda x: None if x < 0 else x))

omitted = [n for n in df.columns if n not in period_df.columns]
for o in omitted:
    if df[o].dtype == pl.Utf8 or df[o].dtype == pl.Int64:
        period_df = period_df.with_columns(
            pl.lit(None, dtype=df[o].dtype).alias(o))
    else:
        period_df = period_df.with_columns(
            pl.lit(np.NaN, dtype=df[o].dtype).alias(o))

df = df.extend(period_df.select(df.columns))

if paper:
    df = df.filter(pl.col("tool").is_in(figure_tools + table_tools))

    df = df.filter(~pl.col("benchmark").is_in([
        "Inspect_benchmarks/bbuf", # duplicate
        "RADBench/bug1",
        "RADBench/bug2",
        "RADBench/bug3",
    ]))
    # print((df.to_pandas()["benchmark"].unique()))
    # print(len(df.to_pandas()["benchmark"].unique()))

    tool_mapping = {
        "pos-only-schedfuzz": "POS",
        ours: "RFF",
        "no-rff-no-afl-cov-always-rand-schedfuzz": "RAIDFuzzer (−greybox feedback)",
        "qlearning-schedfuzz": "QLearning RF",
        "pct-no-rff-always-rand-schedfuzz": "PCT3",
        "PERIOD": "PERIOD",
        "GenMC": "GenMC",
    }

    figure_tools = list(map(tool_mapping.get, figure_tools))
    df = df.with_columns(pl.col("tool").replace(tool_mapping, default=pl.first()))

    ours = tool_mapping[ours]

num_trials = df["trial_num"].max() + 1

found = df.with_columns( (pl.col("buggy_scheds") > 0).alias("found"))

## Per Bug Survival
variables = ["scheds_to_bug", "time_to_bug"]
defaults = [pl.col("total_scheds"), 300]


def add_run_col(df, variable, default):
    r2 = df.with_columns(
         pl.when(pl.col(variable).is_null()) \
            .then(default).otherwise(pl.col(variable)) \
            .alias(variable + "_run"))
    return r2

df = found
for (v, d) in zip(variables, defaults): 
    df = add_run_col(df, v, d)

# --------------------------------------------- 
def plot_bench_survival(df, bench, variable, default):
    df = df.filter(pl.col("benchmark") == bench)

    ax = plt.axes()
    for tool in df["tool"].unique():
        # No time for PERIOD yet...
        if variable == "time_to_bug" and tool == "PERIOD":
            continue

        if not ("RFF" in tool):
            continue

        d = df.filter(pl.col("tool") == tool).to_pandas()
        d = d.dropna(subset = ['total_scheds'])

        kmf = KaplanMeierFitter(label = tool)

        kmf.fit(d[variable + "_run"], d["found"])
        # rmst = restricted_mean_survival_time(kmf, t=limit)
        # print(f"rmst {rmst}")
        kmf.plot(ax = ax)

bench = "Chess/StateWorkStealQueue"
bench = "Splash2/lu"
bench = "CS/twostage_100"
bench = "CS/bluetooth_driver"
bench = "CS/reorder_20"
bench = "ConVul-CVE-Benchmarks/CVE-2013-1792"
bench = "Chess/InterlockedWorkStealQueueWithState"
bench = "Inspect_benchmarks/qsort_mt"

# plot_bench_survival(df, bench, variables[0], defaults[0])
# plt.xlabel("Schedules Explored")
# plt.ylabel("Probability Bug is Not Yet Found")
# plt.savefig(f'assets/qsort.png', bbox_inches="tight", dpi = 200)
# plt.show()
    
# --------------------------------------------- 

pvals = []
per_b_sigs = []

variable = variables[0]
tools = df["tool"].unique()
for bench in df["benchmark"].unique():
    b_sigs = []
    for (t1, t2) in itertools.combinations(tools, 2):

        t_df = df.to_pandas()
        b_df = t_df[t_df["benchmark"] == bench]

        t1_dur = b_df[b_df["tool"] == t1][variable + "_run"]
        t2_dur = b_df[b_df["tool"] == t2][variable + "_run"]

        t1_found = b_df[b_df["tool"] == t1]["found"]
        t2_found = b_df[b_df["tool"] == t2]["found"]

        if len(t1_found) > 0 and len(t2_found) > 0:
            r = logrank_test(t1_dur, t2_dur, t1_found, t2_found);

            pvals.append({"t1": t1, "t2": t2, "benchmark": bench, "pval": r.p_value})
            pvals.append({"t1": t2, "t2": t1, "benchmark": bench, "pval": r.p_value})

            if r.p_value < 0.05:
                b_sigs.append((t1, t2)) 
    per_b_sigs.append({"benchmark": bench, "sig_diffs": b_sigs})


pvals_df = pd.DataFrame(pvals)
per_b_sigs_df = pd.DataFrame(per_b_sigs)

def lookup_sig(t1, t2, b):
    f1 = pvals_df[pvals_df["t1"] == t1]
    f2 =  f1[f1["t2"] == t2]
    f3 =  f2[f2["benchmark"] == b]
    return f3.pval

per_b_sigs_df.to_csv("per_b_sig.csv")

agg_nf = (found.groupby(["benchmark", "tool"]) \
    .agg(mean_scheds = pl.col("scheds_to_bug").mean(),
         std_scheds = pl.col("scheds_to_bug").std(),
         find_prob = pl.col("found").sum()/num_trials))

agg = agg_nf.with_columns(
     pl.when(pl.col("find_prob") < 1) \
        .then(None).otherwise(pl.col("mean_scheds")) \
        .alias("mean_scheds") )

agg = agg.with_columns(
     pl.when(pl.col("find_prob") < 1) \
        .then(None).otherwise(pl.col("std_scheds")) \
        .alias("std_scheds") )

def formatter(row):
    b = row["benchmark"]
    t = row["tool"]
    var = "scheds"
    my_mean = row["mean_scheds"]

    not_best = False
    anf_b = agg_nf[agg_nf["benchmark"] == b]
    for t2 in agg_nf["tool"].unique():
        if not t2 == t:
            other_mean = anf_b[anf_b["tool"] == t2]["mean_scheds"]
            if (len(other_mean) > 0) and (other_mean.item() < my_mean):
                # may not be best...
                sig = lookup_sig(t, t2, b)
                if not not_best:
                    not_best = sig.item() < 0.05
        

    print(f"r is {row}")
    if np.isnan(row["mean_scheds"]):
        s = f'({np.round(row["mean_scheds"])})'
    else:
        s = f'{np.round(row["mean_scheds"])} $\pm$ {np.round(row["std_scheds"])}'
    if 0 < row["find_prob"] < 1:
        s = s + "*"

    if not not_best:
        s = f"\\textbf{{ {s} }}"
    return s
        

agg_nf = agg_nf.to_pandas()
agg_nf["Schedules to 1st Bug"] = agg_nf.apply(formatter, axis=1)
agg_nf = (agg_nf.set_index(["benchmark", "tool"]).T.stack().T)

prior = pd.read_csv("scripts/data-analysis/prior_work.csv")
prior = prior.set_index("benchmark")

new = (agg_nf["Schedules to 1st Bug"]
    .merge(prior, left_on="benchmark", right_on="benchmark") )
if paper:
    print(figure_tools)
    new = new[figure_tools + table_tools]


print(new.to_latex())

for_csv = (agg.to_pandas().set_index(["benchmark", "tool"]).T.stack().T)
for_csv.to_csv("agg-data-better.csv")

bug_counts = found.groupby(["tool", "trial_num"]).agg(pl.col("found").sum())

means = found.groupby(["tool"]).agg(pl.col("found").mean())
print(means)


bcp = bug_counts.to_pandas()
print(bcp)
print(bcp.groupby("tool").mean())
# u, p = mannwhitneyu(bcp[bcp["tool"] == ours]["found"], bcp[bcp["tool"] == "PERIOD"]["found"], method="exact")
# print(f"MWU p-val is {p}")
# exit()


## Overall Bug Count
ax = sns.violinplot(bug_counts.to_pandas(), y = "tool", x = "found", width = 1.5, order=figure_tools)

# plt.xlim(xmin=0)
ax.set(xlabel='Bugs Found', ylabel=None)

# plt.savefig(f'assets/violin.png', bbox_inches="tight", dpi = 200)

# also can use boxplot here
# note, interquartile range != 95% CI
# still need wilcoxon
# sns.boxplot(bug_counts.to_pandas(), y = "tool", x = "found")
# plt.xlim(xmin=0)
# plt.show()




def compute_rmsts(df, variable, limit):
    rmsts = [] 
    for b in set(df["benchmark"]):
        # for prototyping
        if b != "Inspect_benchmarks/qsort_mt":
            continue
        for t in set(df["tool"]):
            if t == "PERIOD" and variable == "time_to_bug":
                continue

            d = df.filter(pl.col("tool") == t) \
              .filter(pl.col("benchmark") == b) \
              .to_pandas() 

            d = d.dropna(subset = ['total_scheds'])
            if len(d) > 0:
                kmf = KaplanMeierFitter()
                kmf.fit(d[variable + "_run"], d["found"])
                rmst = restricted_mean_survival_time(kmf, t=limit, return_variance=True)
                row = {
                    "benchmark": b,
                    "tool": t,
                    "rmst": rmst[0],
                    "rmst_variance": rmst[1],
                }
                print(row)


variable = "scheds_to_bug"
limit = 300
# compute_rmsts(df, variable, limit)

## Cumulative Bugs per Sched Scatter
plt.clf()
# cum_scheds = found.filter(pl.col("error").is_null())
cum_scheds = found
cum_scheds = (cum_scheds.sort(["tool", "trial_num", "scheds_to_bug"]).groupby(["tool", "trial_num"]) \
    .apply(lambda g: g.with_columns(pl.col("found").cumsum())))
cum_scheds = (cum_scheds.select(["tool", "trial_num", "scheds_to_bug", "found"]))
cum_scheds = (cum_scheds.filter(pl.col("found") > 0))
cum_scheds = cum_scheds.groupby("tool", "trial_num", "scheds_to_bug").agg(pl.col("found").max())
cum_scheds = cum_scheds.with_columns(pl.col("scheds_to_bug").apply(lambda x: math.log(x) if x else x))

ax = sns.lmplot(
    cum_scheds.to_pandas(),
    x = "scheds_to_bug",
    y = "found",
    hue="tool",
    x_ci="ci",
    # markers = ['+', '*', '.', 'x', '1'],
    logx = False,
    fit_reg = False,
    legend_out = False,
    hue_order = figure_tools,
)

ax.set(xlabel='(log) Schedules Explored', ylabel='Cumulative Bugs Found')

plt.savefig(f'assets/cum-scheds-to-bug.png', bbox_inches="tight", dpi = 200)
