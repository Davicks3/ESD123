% =========================================================================
% MATLAB Script to Plot Transmission Time Intervals
% File required: "Sendt Data.txt" in the same directory
% =========================================================================

% 1. Setup and File Reading
filename = 'SendtData.txt';

fprintf('Reading data from %s...\n', filename);

% Open the file
fid = fopen(filename, 'r');

% Parse the file. 
% Format 'Sent: %f' tells MATLAB to ignore "Sent: " and read the number as a float.
data = textscan(fid, 'Sent: %f');

% Close the file
fclose(fid);

% Extract the timestamps (Cell array to vector)
timestamps_us = data{1};

% 2. Calculate Intervals
% diff() calculates the difference between adjacent elements: t(2)-t(1), t(3)-t(2), etc.
intervals_us = diff(timestamps_us); 

% Optional: Convert to milliseconds for easier reading (uncomment if needed)
% intervals_ms = intervals_us / 1000;

% 3. Calculate Statistics
avg_interval = mean(intervals_us);
min_interval = min(intervals_us);
max_interval = max(intervals_us);
jitter = std(intervals_us); % Standard deviation is often used as a jitter metric

fprintf('--------------------------------------------------\n');
fprintf('Statistics:\n');
fprintf('Total Packets: %d\n', length(timestamps_us));
fprintf('Average Interval: %.2f microseconds\n', avg_interval);
fprintf('Min Interval:     %.2f microseconds\n', min_interval);
fprintf('Max Interval:     %.2f microseconds\n', max_interval);
fprintf('Jitter (Std Dev): %.2f microseconds\n', jitter);
fprintf('--------------------------------------------------\n');

% 4. Plotting
% 'Color', 'w' sets the figure window background to white
fig = figure('Name', 'Transmission Analysis', 'Color', 'w');

% Subplot 1: The Time Intervals
subplot(2,1,1);
plot(intervals_us, '-b.', 'LineWidth', 1, 'MarkerSize', 8);
grid on;
% Ensure axes background is white and text is black
set(gca, 'Color', 'w', 'XColor', 'k', 'YColor', 'k'); 
title('Transmission Time Interval (TTI) between Packets', 'Color', 'k');
xlabel('Packet Index', 'Color', 'k');
ylabel('Interval (\mus)', 'Color', 'k');
legend({'Interval per packet'}, 'TextColor', 'k');
xlim([1 length(intervals_us)]);

% Subplot 2: Histogram (Distribution of intervals)
subplot(2,1,2);
histogram(intervals_us, 50, 'FaceColor', [0.2 0.6 0.5]);
grid on;
% Ensure axes background is white and text is black
set(gca, 'Color', 'w', 'XColor', 'k', 'YColor', 'k');
title('Distribution of Time Intervals', 'Color', 'k');
xlabel('Interval Duration (\mus)', 'Color', 'k');
ylabel('Frequency (Count)', 'Color', 'k');

fprintf('Plot generated successfully.\n');