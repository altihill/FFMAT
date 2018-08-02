if ispc
    [sourcepath,~,~] = fileparts(which('ffmat.c'));
else
    sourcepath = '/usr/local';
end
mex('ffmat.c',['-I' sourcepath '/include'],['-L' sourcepath '/lib'],...
    '-lavcodec','-lavformat','-lavutil','-lswscale');