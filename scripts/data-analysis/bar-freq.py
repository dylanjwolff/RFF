import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import os


os.makedirs("assets", exist_ok=True)

fnames = [
    "scripts/data-analysis/freq-data/power-schedfuzz-exact-rfs.csv",
    "scripts/data-analysis/freq-data/pos-exact-rfs.csv",
]

def get_counts(fname, normalize=False):
    basename = os.path.splitext(os.path.basename(fname))[0]

    df = pd.read_csv(fname, header=None)
    df.columns = ["r", "w"]
    df["rf"] = df[["r", "w"]].apply(tuple, axis=1)

    counts = pd.DataFrame(df.groupby(["rf"])["rf"].count())
    counts.columns = ["counts"]
    counts = counts.reset_index()
    if normalize:
        counts["counts"]=(counts["counts"]-counts["counts"].min())/(counts["counts"].max()-counts["counts"].min())
    counts["strategy"] = basename
    return counts

counts = [get_counts(fname) for fname in fnames]
counts = pd.concat(counts)
counts = counts.sort_values("counts")

counts["rf"] = counts["rf"].map(lambda x: str(x))

sns.barplot(data=counts, x="rf", y="counts", hue="strategy")
plt.xticks(rotation=45)
# plt.show()

plt.savefig(f'assets/rf-pair-bar.png', bbox_inches="tight", dpi = 200)


fnames = [
    "scripts/data-analysis/freq-data/power-schedfuzz-paths.csv",
    "scripts/data-analysis/freq-data/pos-paths.csv",
]

def get_path_counts(fname, normalize=False):
    basename = os.path.splitext(os.path.basename(fname))[0]

    df = pd.read_csv(fname, header=None)
    df.columns = ["id"]
    counts = pd.DataFrame(df.groupby(["id"])["id"].count())
    counts.columns = ["counts"]
    counts = counts.reset_index()

    if normalize:
        counts["counts"]=(counts["counts"]-counts["counts"].min())/(counts["counts"].max()-counts["counts"].min())
    counts["strategy"] = basename
    return counts

counts = [get_path_counts(fname) for fname in fnames]
counts = pd.concat(counts)
counts = counts.sort_values("counts")

# wide_counts = (counts.pivot_table(index=["id"], columns=["strategy"], values=["counts"])) \
#     .sort_values( by = [("counts", "power-schedfuzz-paths"), ("counts", "pos-paths")], ascending=False)
# g = wide_counts.plot(kind='bar', stacked=True)
# print(wide_counts.sum())

g = sns.barplot(data=counts, x="id", y="counts", hue="strategy", order=counts.sort_values("counts")["id"].unique())

id_free_pos = (counts[counts["strategy"] == "pos-paths"]
    .sort_values("counts", ascending=False)
    .reset_index()
    .drop(["id", "index"], axis="columns")
    .head(100)
).reset_index()

id_free_power = (counts[counts["strategy"] == "power-schedfuzz-paths"]
    .sort_values("counts", ascending=False)
    .reset_index()
    .drop(["id", "index"], axis="columns")
    .head(100)
).reset_index()

fig, axes = plt.subplots(2, 1, sharey=True, sharex=True)

g = sns.barplot(ax=axes[0], x=id_free_pos["index"], y=id_free_pos["counts"], color="orange")
g.tick_params(bottom=False)
g.set_xlabel(None)
g.set_ylabel(None)

g = sns.barplot(ax=axes[1], x=id_free_power["index"], y=id_free_power["counts"], color="blue")

g.set(xticklabels=[])
g.tick_params(bottom=False)
g.set_yscale("log")
g.set_xlabel("Top 100 Unique Reads-From Sequences")
g.set_ylabel(None)

plt.savefig(f'assets/bar.png', bbox_inches="tight", dpi = 200)

# id_free = id_free_pos.join(id_free_power, on=["index"], how="outer", lsuffix="_pos", rsuffix="_power")
# print(id_free)

exit()
# print(id_free)
# g = id_free.reset_index()[["counts_pos", "counts_power"]].plot(kind='bar', stacked=True)

# g = sns.barplot(data=id_free_pos, x="index", y="counts", color="blue")
# g = sns.barplot(data=id_free_power, x="index", y="counts", color="orange")

# g = sns.FacetGrid(counts, row="strategy")
# g.map_dataframe(sns.barplot, x="id", y="counts", order=counts.sort_values("counts")["id"].unique()).set(yscale="log", ylim=(0,10000)) 

g.set_yscale("log")
plt.xticks(rotation=45)
plt.show()
