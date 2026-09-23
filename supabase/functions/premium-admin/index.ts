import "jsr:@supabase/functions-js/edge-runtime.d.ts";
import { createClient } from "jsr:@supabase/supabase-js@2";

const cors = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type",
  "Access-Control-Allow-Methods": "POST, OPTIONS"
};

const response = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), {
    status,
    headers: { ...cors, "Content-Type": "application/json" }
  });

Deno.serve(async (req: Request) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });

  try {
    const authHeader = req.headers.get("Authorization");
    if (!authHeader?.startsWith("Bearer ")) return response({ error: "Missing authorization" }, 401);

    const url = Deno.env.get("SUPABASE_URL")!;
    const serviceKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
    const adminClient = createClient(url, serviceKey, {
      auth: { autoRefreshToken: false, persistSession: false }
    });

    const token = authHeader.slice(7);
    const { data: userData, error: userError } = await adminClient.auth.getUser(token);
    if (userError || !userData.user) return response({ error: "Invalid session" }, 401);

    const actorId = userData.user.id;
    const { data: admin, error: adminError } = await adminClient
      .from("premium_admins")
      .select("role")
      .eq("user_id", actorId)
      .maybeSingle();

    if (adminError || !admin || !["owner", "admin"].includes(admin.role)) {
      return response({ error: "Admin access required" }, 403);
    }

    const body = await req.json();
    const action = String(body.action ?? "");
    const targetUserId = String(body.user_id ?? "");

    if (!/^[0-9a-f-]{36}$/i.test(targetUserId)) {
      return response({ error: "A valid user_id is required" }, 400);
    }

    if (action === "grant") {
      const { data, error } = await adminClient.from("premium_subscriptions").upsert({
        user_id: targetUserId,
        status: "active",
        plan: String(body.plan ?? "premium"),
        started_at: body.started_at ?? new Date().toISOString(),
        expires_at: body.expires_at ?? null
      }).select().single();

      if (error) throw error;
      return response({ success: true, subscription: data });
    }

    if (action === "revoke") {
      const { data, error } = await adminClient.from("premium_subscriptions")
        .update({ status: "inactive", expires_at: new Date().toISOString() })
        .eq("user_id", targetUserId)
        .select().single();

      if (error) throw error;
      return response({ success: true, subscription: data });
    }

    if (action === "credit") {
      const amount = Number(body.amount);
      const reason = String(body.reason ?? "admin adjustment").slice(0, 120);

      if (!Number.isInteger(amount) || amount === 0 || Math.abs(amount) > 1000000) {
        return response({ error: "amount must be a non-zero integer within ±1000000" }, 400);
      }

      const { data: current, error: currentError } = await adminClient
        .from("premium_credits")
        .select("balance,lifetime_earned")
        .eq("user_id", targetUserId)
        .maybeSingle();

      if (currentError) throw currentError;

      const oldBalance = current?.balance ?? 0;
      const newBalance = oldBalance + amount;
      if (newBalance < 0) return response({ error: "Credit balance cannot go below zero" }, 400);

      const lifetimeEarned = (current?.lifetime_earned ?? 0) + Math.max(amount, 0);

      const { data, error } = await adminClient.from("premium_credits").upsert({
        user_id: targetUserId,
        balance: newBalance,
        lifetime_earned: lifetimeEarned
      }).select().single();

      if (error) throw error;

      const { error: txError } = await adminClient.from("premium_credit_transactions").insert({
        user_id: targetUserId,
        delta: amount,
        reason
      });

      if (txError) throw txError;

      return response({ success: true, credits: data });
    }

    return response({ error: "Unknown action" }, 400);
  } catch (error) {
    return response({ error: error instanceof Error ? error.message : "Server error" }, 500);
  }
});
