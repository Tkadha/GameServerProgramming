myid = 99999;
atk = -99999;
function set_uid(x)
   myid = x;
end
function set_atk(x)
   atk = x;
end

function event_player_move(player)
   player_x = API_get_x(player);
   player_y = API_get_y(player);
   my_x = API_get_x(myid);
   my_y = API_get_y(myid);
   if (player_x == my_x) then
      if (player_y == my_y) then
         API_ChangeStat(myid, player, atk);
      end
   end
end
