for f in directory
                      path = folder*f
                      ins = read_dat(path)
                  
                      res_path = "/home/francesco/smspp-project/tests/LagrangianDualSolver_MMCF/NEW_ICML/MAX-5/"*init*"/"*string(iters)*"/"
                      time_path = res_path*"times/"
                      lm_path = res_path*"redCosts/"
                      
                      name = split(f,".")[1]*"_2Sol-"
                      f_t=open(time_path*name*"time.dat","r")
                      line=f*"   "*string(obj)*"   "*readline(f_t)*"   "*readline(f_t)
                      println(line)
                      close(f_t)
               end
               
times, objvalues, iters=[],[],[]
for f in directory
                 if contains(f,"1Sol") 
                    continue
                 end;        
                 ff=open(folder*f,"r")
                 append!(times,parse(Float32,readline(ff)));
                 append!(objvalues,parse(Float32,readline(ff)));
                 append!(iters,parse(Int64,readline(ff)))
                 close(ff)
                 println(f," ",times[end], " ", objvalues[end])
end

