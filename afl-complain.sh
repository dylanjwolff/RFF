echo core >/proc/sys/kernel/core_pattern
cd /sys/devices/system/cpu; echo performance | tee cpu*/cpufreq/scaling_governor
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space



