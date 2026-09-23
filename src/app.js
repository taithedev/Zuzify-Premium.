import { createClient } from "https://esm.sh/@supabase/supabase-js@2";
import { SUPABASE_URL, SUPABASE_PUBLISHABLE_KEY } from "./config.js";

const accountState=document.querySelector("#accountState");
const signOut=document.querySelector("#signOut");
const premiumStatus=document.querySelector("#premiumStatus");
const accountPanel=document.querySelector("#accountPanel");
const getStarted=document.querySelector("#getStarted");
const manageAccount=document.querySelector("#manageAccount");
const saveSettings=document.querySelector("#saveSettings");
const accentColor=document.querySelector("#accentColor");
const glassIntensity=document.querySelector("#glassIntensity");
const message=document.querySelector("#message");
const supabase=SUPABASE_URL&&SUPABASE_PUBLISHABLE_KEY?createClient(SUPABASE_URL,SUPABASE_PUBLISHABLE_KEY):null;
let currentUser=null;
function applySettings(){document.documentElement.style.setProperty("--accent",accentColor.value);document.documentElement.style.setProperty("--glass",glassIntensity.value+"px")}
async function updateAccount(){if(!currentUser){accountState.textContent="Not signed in";signOut.classList.add("hidden");premiumStatus.textContent="Not active";return}accountState.textContent=currentUser.email||"Signed in";signOut.classList.remove("hidden");const {data,error}=await supabase.from("premium_subscriptions").select("status,expires_at").eq("user_id",currentUser.id).maybeSingle();if(error){premiumStatus.textContent="Unavailable";return}const active=data?.status==="active"&&(!data.expires_at||new Date(data.expires_at)>new Date());premiumStatus.textContent=active?"Active":"Not active"}
async function loadSession(){if(!supabase){accountState.textContent="Supabase not configured";premiumStatus.textContent="Setup required";return}const {data:{session}}=await supabase.auth.getSession();currentUser=session?.user||null;await updateAccount()}
getStarted.addEventListener("click",()=>{accountPanel.classList.remove("hidden");accountPanel.scrollIntoView({behavior:"smooth",block:"center"})});
manageAccount.addEventListener("click",()=>{accountPanel.classList.toggle("hidden");accountPanel.scrollIntoView({behavior:"smooth",block:"center"})});
signOut.addEventListener("click",async()=>{if(!supabase)return;await supabase.auth.signOut();currentUser=null;await updateAccount()});
accentColor.addEventListener("input",applySettings);
glassIntensity.addEventListener("input",applySettings);
saveSettings.addEventListener("click",()=>{message.textContent=!supabase||!currentUser?"Sign in after Supabase is configured.":"Premium settings are ready for the account database layer."});
supabase?.auth.onAuthStateChange(async(_event,session)=>{currentUser=session?.user||null;await updateAccount()});
applySettings();
loadSession();
