clc
clear all
close all
dbstop if error

%     try
% import matlab.net.*
% import matlab.net.http.*


url='http://192.168.0.177';
% url='http://10.24.1.3';

for j=1:100
    j
%     [stat,h]=web(url,'-browser');
    [stat,h]=web(url);
    
    pause(7)
%     for j=1:10
    data=get(h,'HtmlText');
%     pause(0.5);
%     end
    %     set(h,'Enabled',0)
    %     close(h)
    %     pause(1)
    %
    % end
    

    
    word='samples';
    try
        I=strfind(data,word)+length(word);
        Iend=strfind(data,'</pre>');
        data_num=data(I(1):Iend-1);
        str=splitlines(data_num);
        
        histogram=[];
        for i=1:size(str,1)
            temp=str{i};
            I=strfind(temp,',');
            
            if ~isempty(I)
                
                histogram=[histogram;[str2num(temp(1:I-1)),str2num(temp(I+1:end))]]
                
                %         keyboard
            end
            
            if(strfind(temp,'Ohm'))
                Resistance=str2double(temp(1:4));
            end
            
        end
        
        figure(1)
        subplot(2,1,1)
        plot(histogram(:,1),histogram(:,2),'*')
        xlabel('windspeed (km/h)')
        ylabel('histogram')
        grid on
        
        subplot(2,1,2)
        
        plot(0,Resistance)
        ylabel('kOhm')
        clear data
        
        load handel;
        player = audioplayer(y, Fs);
        play(player)
    end
    
    %     keyboard
    set(h,'Enabled',0)
    close(h)
    
    pause(5)
end

% for i=1:100

% r = RequestMessage;
% uri = URI('http://192.168.0.206');
% resp = send(r,uri)

%     t=tcpclient('192.168.0.206', 80,'Timeout', 20) ;
%
%     write(t,0)
%     end
%     pause(0.1)
%     t=tcpclient('192.168.0.206', 80,'Timeout', 20);

%     if t.BytesAvailable>0
%
%         data=read(t)
%     end

% end
