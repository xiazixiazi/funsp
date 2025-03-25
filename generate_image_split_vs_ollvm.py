
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("output/test_time_consumption_result.csv")
df2 = pd.read_csv("output/filesize_diff.csv")

df["time-split"] = df["clang_O0_split_mean"] / df["clang_O0"]
df["time-ollvm"] = df["obfuscator-llvm-O0-sub-fla-bcf"] / df["clang_O0"]
df_ratio = df[["program", "time-split", "time-ollvm"]]

df2["filesize-split"] = df2["clang_O0_split_mean"] / df2["clang_O0"]
df2["filesize-ollvm"] = df2["obfuscator-llvm-O0-sub-fla-bcf"] / df2["clang_O0"]
df2_ratio = df2[["program", "filesize-split", "filesize-ollvm"]]

merged_df = pd.merge(df_ratio, df2_ratio, on='program', how='inner')
merged_df

plt.figure(figsize=(12, 8))

plt.scatter(
    x=merged_df["time-split"],
    y=merged_df["filesize-split"],
    c="blue",
    s=80,
    alpha=0.7,
    edgecolors="k",
    label="Split"
)

plt.scatter(
    x=merged_df["time-ollvm"],
    y=merged_df["filesize-ollvm"],
    c="red",
    s=80,
    alpha=0.7,
    edgecolors="k",
    label="OLLVM"
)

plt.legend(
    loc="upper right",
    fontsize=12,
    title="Group",
    title_fontsize=12,
    framealpha=0.9
)

plt.xlabel("Time Ratio", fontsize=12)
plt.ylabel("Filesize Ratio", fontsize=12)
plt.title("Comparison of Split vs OLLVM", fontsize=14, pad=20)

plt.xlim(0, 8)
plt.ylim(0, 8)

plt.grid(True, alpha=0.3)

plt.tight_layout()
# plt.show()
plt.savefig("output/Comparison of Split vs OLLVM.svg")
