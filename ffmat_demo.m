% ffmat_demo by Gao Shan, 2017

[VideoFileName,VideoPathName] = uigetfile('*.*','Please select the video file');
Output_H = 720;
Output_W = 1280;

% open video
Status = ffmat('openvideo',[VideoPathName VideoFileName],Output_W,Output_H,'Gray');
if Status<0
    error('File Open Failed');
end

% get property
[Status,Prop] = ffmat('getprop');
if Status>0
    disp(Prop);
end

% sequentially read frame
NumFrame = 100;
figure;
hi = imshow(zeros(Output_H,Output_W,'uint8'));
ts = zeros(NumFrame,1);
tic
for ii = 1:NumFrame
    [ts(ii),RawData] = ffmat('readframe');
    set(hi,'CData',RawData);
    pause(realmin);
end
toc
figure,plot(ts);

% speed test
NumFrame = 1000;
ts = zeros(NumFrame,1);
tic
for ii = 1:NumFrame
    [ts(ii),~] = ffmat('readframe');
end
timespend = toc;
disp(['Reading FPS: ' num2str(NumFrame/timespend)]);
figure,plot(ts);

% pick frame
NumFrame = 100;
seed = round(rand(NumFrame,1)*Prop.TotalFrames)+1;
figure;
hi = imshow(zeros(Output_H,Output_W,'uint8'));
ts = zeros(NumFrame,1);
tic
for ii = 1:NumFrame
    [ts(ii),RawData] = ffmat('pickframe',seed(ii));
    set(hi,'CData',RawData);
    pause(realmin);
end
toc
figure,plot(ts./max(ts));
hold on,plot(seed./max(seed)-0.01);

% seek to specific frame
TargetFrame = 16;
CurrentTime = ffmat('seekframe',TargetFrame);
[~,Prop] = ffmat('getprop');
fprintf('target frame: %d\nnext frame: %d\n',TargetFrame,Prop.NextFrame);
[ts,RawData] = ffmat('readframe');
figure,imshow(RawData);

% close video
Status = ffmat('closevideo');
if Status<0
    error('File Close Failed');
end
