% ffmat_demo by Gao Shan, 2018

pixfmt = 'Gray';
sz = [720 1280 1];
[vfn,vpn] = uigetfile('*.*','Please select the video file');

% open video
ret = ffmat('openvideo',fullfile(vpn,vfn),sz(2),sz(1),pixfmt);
if ret<0
    error('File Open Failed');
end

% get property
[ret,prop] = ffmat('getprop');
if ret>0
    disp(prop);
else
    error('Property Get Failed');
end

% sequentially read frame
n = 100;
figure;
hi = imshow(zeros(sz,'uint8'));
ht = text(0,-10,'Timestamp: 0.00 s');
ts = zeros(n,1);
for ii = 1:n
    [ts(ii),frame] = ffmat('readframe');
    if ts(ii)>-1
        set(hi,'CData',frame);
        set(ht,'String',sprintf('Timestamp: %.2f s',ts(ii)));
        pause(realmin);
    else
        error('Frame Read Failed');
    end
end
[~,prop] = ffmat('getprop');
figure;
plot(prop.NextFrame-n:prop.NextFrame-1,ts,'.');
xlabel('Frame Number');
ylabel('Timestamp (s)');

% sequential speed test
n = 1000;
tic
for ii = 1:n
    [~,~] = ffmat('readframe');
end
timespend = toc;
disp(['Sequential reading FPS: ' num2str(n/timespend)]);

% sparsely pick frame
n = 100;
[~,prop] = ffmat('getprop');
seed = randi(prop.TotalFrames,n,1);
figure;
hi = imshow(zeros(sz,'uint8'));
ht = text(0,-10,'Timestamp: 0.00 s');
ts = zeros(n,1);
for ii = 1:n
    [ts(ii),frame] = ffmat('pickframe',seed(ii));
    if ts(ii)>-1
        set(hi,'CData',frame);
        set(ht,'String',sprintf('Timestamp: %.2f s',ts(ii)));
        pause(realmin);
    else
        error('Frame Pick Failed');
    end
end
figure;
xlim([0 n+1]);
yyaxis left, plot(seed,'o'), ylabel('Frame Number'), ylim([0 prop.TotalFrames]);
yyaxis right, plot(ts,'.'), ylabel('Timestamp (s)'), ylim([0 prop.Duration]);

% sparse speed test
n = 100;
[~,prop] = ffmat('getprop');
seed = randi(prop.TotalFrames,n,1);
tic
for ii = 1:n
    [~,~] = ffmat('pickframe',seed(ii));
end
timespend = toc;
disp(['Sparse reading FPS: ' num2str(n/timespend)]);

% seek to specific frame
TargetFrame = randi(prop.TotalFrames);
ts = ffmat('seekframe',TargetFrame);
if ts > -1
    [~,prop] = ffmat('getprop');
    [ts,frame] = ffmat('readframe');
    figure,imshow(frame);
    text(0,-10,sprintf(...
        'Seek Target: %d  Frame Got: %d  Frame timestamp: %.2f s',...
        TargetFrame,prop.NextFrame,ts));
else
    error('Frame Seek Failed');
end

% close video
ret = ffmat('closevideo');
if ret<0
    error('File Close Failed');
end