create or replace function public.handle_premium_user_created()
returns trigger
language plpgsql
security definer
set search_path = public
as $$
begin
  insert into public.premium_settings (user_id)
  values (new.id)
  on conflict (user_id) do nothing;

  insert into public.premium_credits (user_id)
  values (new.id)
  on conflict (user_id) do nothing;

  return new;
end;
$$;

drop trigger if exists on_auth_user_created_premium on auth.users;
create trigger on_auth_user_created_premium
after insert on auth.users
for each row execute function public.handle_premium_user_created();
