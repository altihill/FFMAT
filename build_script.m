sourcepath = '/usr/local';
mex('ffmat.cpp',['-I' sourcepath '/include'],['-L' sourcepath '/lib'],...
    '-lavcodec','-lavformat','-lavutil','-lswscale');

