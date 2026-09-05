/*
31	 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
32	 *
33	 * SPDX-License-Identifier: LGPL-3.0-or-later
34	 */
35
36	#include <gtest/gtest.h>
37
38	#include "../../common/scoped_umask.h"
39	#include "../../common/tempdir.h"
40	#include "linglong/api/types/v1/Generators.hpp"
41	#include "linglong/api/types/v1/PackageInfoV2.hpp"
42	#include "linglong/api/types/v1/Repo.hpp"
43	#include "linglong/api/types/v1/RepoConfigV2.hpp"
44	#include "linglong/api/types/v1/RepositoryCache.hpp"
45	#include "linglong/package/reference.h"
46	#include "linglong/repo/config.h"
47	#include "linglong/repo/remote_packages.h"
48	#include "linglong/repo/repo_cache.h"
49
50	#include <filesystem>
51	#include <fstream>
52	#include <vector>
53
54	namespace linglong::repo::test {
55
56	namespace fs = std::filesystem;
57
58	namespace {
59
60	api::types::v1::PackageInfoV2
61	makePkgInfo(std::string id, std::string version, std::string channel = "main", std::string mod = "binary")
62	{
63	    return api::types::v1::PackageInfoV2{
64	        .arch = std::vector<std::string>{ "x86_64" },
65	        .channel = std::move(channel),
66	        .id = std::move(id),
67	        .kind = "app",
68	        .packageInfoV2Module = std::move(mod),
69	        .version = std::move(version),
70	    };
71	}
72
73	api::types::v1::RepositoryCacheLayersItem
74	makeLayer(std::string commit,
75	          std::string id,
76	          std::string version,
77	          std::string repoName = "stable",
78	          std::optional<bool> deleted = std::nullopt)
79	{
80	    return api::types::v1::RepositoryCacheLayersItem{
81	        .commit = std::move(commit),
82	        .deleted = deleted,
83	        .info = makePkgInfo(std::move(id), std::move(version)),
84	        .repo = std::move(repoName),
85	    };
86	}
87
88	class RepoDeepSuiteTest : public ::testing::Test
89	{
90	protected:
91	    TempDir tempDir;
92	};
93
94	TEST_F(RepoDeepSuiteTest, RepoCacheQueryToStringFormat)
95	{
96	    repoCacheQuery q1{
97	        .id = "org.deepin.browser",
98	        .repo = "official",
99	        .channel = "main",
100	        .version = "1.0.0",
101	        .module = "binary",
102	        .architecture = "x86_64",
103	    };
104	    EXPECT_EQ(q1.to_string(), "official:main/org.deepin.browser/1.0.0/x86_64/binary");
105
106	    repoCacheQuery qDefaults{};
107	    std::string strDefaults = qDefaults.to_string();
108	    EXPECT_NE(strDefaults.find("undefined:undefined/undefined/undefined/"), std::string::npos);
109	}
110
111	TEST_F(RepoDeepSuiteTest, RepoCacheMultipleLayerAdditionsAndDeduplication)
112	{
113	    auto cacheFile = tempDir.path() / "cache_dedup.json";
114	    RepoCache cache(cacheFile);
115
116	    auto layer1 = makeLayer("commit-aaa", "org.deepin.demo", "1.0.0.1", "repo-a");
117	    auto layer2 = makeLayer("commit-bbb", "org.deepin.demo", "1.0.0.2", "repo-a");
118	    auto layer3 = makeLayer("commit-ccc", "org.deepin.demo", "1.0.0.3", "repo-b");
119
120	    ASSERT_TRUE(cache.addLayerItem(layer1).has_value());
121	    ASSERT_TRUE(cache.addLayerItem(layer2).has_value());
122	    ASSERT_TRUE(cache.addLayerItem(layer3).has_value());
123
124	    auto allExisting = cache.queryExistingLayerItem();
125	    EXPECT_EQ(allExisting.size(), 3);
126
127	    // Adding exact duplicate item
128	    auto duplicate = layer1;
129	    ASSERT_TRUE(cache.addLayerItem(duplicate).has_value());
130	    auto afterDup = cache.queryExistingLayerItem();
131	    EXPECT_EQ(afterDup.size(), 3);
132	}
133
134	TEST_F(RepoDeepSuiteTest, RepoCacheFuzzyVersionQueryOrdering)
135	{
136	    auto cacheFile = tempDir.path() / "cache_fuzzy.json";
137	    RepoCache cache(cacheFile);
138
139	    ASSERT_TRUE(cache.addLayerItem(makeLayer("c1", "app.calc", "1.2.0.1")).has_value());
140	    ASSERT_TRUE(cache.addLayerItem(makeLayer("c2", "app.calc", "1.2.1.0")).has_value());
141	    ASSERT_TRUE(cache.addLayerItem(makeLayer("c3", "app.calc", "1.3.0.0")).has_value());
142	    ASSERT_TRUE(cache.addLayerItem(makeLayer("c4", "app.calc", "2.0.0.0")).has_value());
143
144	    // Query prefix 1.2.*
145	    repoCacheQuery qPrefix{
146	        .id = "app.calc",
147	        .version = "1.2",
148	    };
149	    auto results = cache.queryLayerItem(qPrefix);
150	    ASSERT_EQ(results.size(), 2);
151	    EXPECT_EQ(results[0].commit, "c2"); // higher version comes first
152	    EXPECT_EQ(results[1].commit, "c1");
153
154	    // Query wildcard / all versions
155	    repoCacheQuery qAll{
156	        .id = "app.calc",
157	    };
158	    auto allResults = cache.queryLayerItem(qAll);
159	    ASSERT_EQ(allResults.size(), 4);
160	    EXPECT_EQ(allResults[0].commit, "c4");
161	    EXPECT_EQ(allResults[1].commit, "c3");
162	    EXPECT_EQ(allResults[2].commit, "c2");
163	    EXPECT_EQ(allResults[3].commit, "c1");
164	}
165
166	TEST_F(RepoDeepSuiteTest, RepoCacheSoftDeleteAndQueryDeletedFilter)
167	{
168	    auto cacheFile = tempDir.path() / "cache_del.json";
169	    RepoCache cache(cacheFile);
170
171	    auto item1 = makeLayer("c1", "app.test", "1.0.0");
172	    auto item2 = makeLayer("c2", "app.test", "2.0.0");
173	    ASSERT_TRUE(cache.addLayerItem(item1).has_value());
174	    ASSERT_TRUE(cache.addLayerItem(item2).has_value());
175
176	    EXPECT_EQ(cache.queryExistingLayerItem().size(), 2);
177
178	    // Delete item1
179	    auto delRes = cache.deleteLayerItem(item1);
180	    ASSERT_TRUE(delRes.has_value());
181
182	    // Existing items should now exclude item1
183	    auto existing = cache.queryExistingLayerItem();
184	    ASSERT_EQ(existing.size(), 1);
185	    EXPECT_EQ(existing.front().commit, "c2");
186
187	    // Query with deleted = true should find item1
188	    repoCacheQuery qDeleted{
189	        .deleted = true,
190	    };
191	    auto deletedItems = cache.queryLayerItem(qDeleted);
192	    ASSERT_EQ(deletedItems.size(), 1);
193	    EXPECT_EQ(deletedItems.front().commit, "c1");
194
195	    // Query with deleted = false should find item2
196	    repoCacheQuery qActive{
197	        .deleted = false,
198	    };
199	    auto activeItems = cache.queryLayerItem(qActive);
200	    ASSERT_EQ(activeItems.size(), 1);
201	    EXPECT_EQ(activeItems.front().commit, "c2");
202	}
203
204	TEST_F(RepoDeepSuiteTest, RepoCacheMergedItemsRoundtrip)
205	{
206	    auto cacheFile = tempDir.path() / "cache_merged.json";
207	    RepoCache cache(cacheFile);
208
209	    EXPECT_FALSE(cache.queryMergedItems().has_value());
210
211	    std::vector<api::types::v1::RepositoryCacheMergedItem> merged;
212	    api::types::v1::RepositoryCacheMergedItem m1{
213	        .commit = "merge-commit-01",
214	        .info = makePkgInfo("org.deepin.runtime", "23.0.0", "main", "runtime"),
215	    };
216	    api::types::v1::RepositoryCacheMergedItem m2{
217	        .commit = "merge-commit-02",
218	        .info = makePkgInfo("org.deepin.base", "23.0.0", "main", "base"),
219	    };
220	    merged.push_back(m1);
221	    merged.push_back(m2);
222
223	    ASSERT_TRUE(cache.updateMergedItems(merged).has_value());
224	    ASSERT_TRUE(cache.writeToDisk().has_value());
225
226	    // Reload from disk
227	    RepoCache cacheReloaded(cacheFile);
228	    ASSERT_TRUE(cacheReloaded.load().has_value());
229	    ASSERT_TRUE(cacheReloaded.queryMergedItems().has_value());
230	    auto reloadedMerged = *cacheReloaded.queryMergedItems();
231	    ASSERT_EQ(reloadedMerged.size(), 2);
232	    EXPECT_EQ(reloadedMerged[0].commit, "merge-commit-01");
233	    EXPECT_EQ(reloadedMerged[1].commit, "merge-commit-02");
234	}
235
236	TEST_F(RepoDeepSuiteTest, RemotePackagesAddAndGetLatestPackage)
237	{
238	    RemotePackages remote;
239	    EXPECT_TRUE(remote.empty());
240
241	    auto errRes = remote.getLatestPackage();
242	    EXPECT_FALSE(errRes.has_value()); // empty packages must return error
243
244	    Repo repo1{
245	        .name = "stable-repo",
246	        .priority = 10,
247	        .url = "https://repo1.deepin.org",
248	    };
249	    Repo repo2{
250	        .name = "beta-repo",
251	        .priority = 20,
252	        .url = "https://repo2.deepin.org",
253	    };
254
255	    std::vector<PackageInfoV2> pkgs1{
256	        makePkgInfo("org.deepin.music", "1.5.0"),
257	        makePkgInfo("org.deepin.music", "2.0.0"),
258	    };
259	    std::vector<PackageInfoV2> pkgs2{
260	        makePkgInfo("org.deepin.music", "2.1.0-beta"),
261	        makePkgInfo("org.deepin.music", "2.0.5"),
262	    };
263
264	    remote.addPackages(repo1, pkgs1);
265	    EXPECT_FALSE(remote.empty());
266	    EXPECT_EQ(remote.getRepoPackages().size(), 1);
267
268	    remote.addPackages(repo2, pkgs2);
269	    EXPECT_EQ(remote.getRepoPackages().size(), 2);
270
271	    auto latestRes = remote.getLatestPackage();
272	    ASSERT_TRUE(latestRes.has_value());
273	    const auto &[latestRepo, latestPkg] = *latestRes;
274	    EXPECT_EQ(latestPkg.get().id, "org.deepin.music");
275	}
276
277	TEST_F(RepoDeepSuiteTest, RemotePackagesGetReferenceModules)
278	{
279	    RemotePackages remote;
280	    Repo repo{
281	        .name = "official",
282	        .priority = 5,
283	        .url = "https://repo.deepin.org",
284	    };
285
286	    std::vector<PackageInfoV2> pkgs{
287	        makePkgInfo("org.deepin.editor", "1.0.0", "main", "binary"),
288	        makePkgInfo("org.deepin.editor", "1.0.0", "main", "develop"),
289	        makePkgInfo("org.deepin.editor", "1.0.0", "main", "doc"),
290	        makePkgInfo("org.deepin.editor", "0.9.0", "main", "binary"),
291	        makePkgInfo("org.deepin.other", "1.0.0", "main", "binary"),
292	    };
293
294	    remote.addPackages(repo, pkgs);
295
296	    auto refParsed = package::Reference::parse("org.deepin.editor/1.0.0/x86_64/binary");
297	    ASSERT_TRUE(refParsed.has_value());
298
299	    auto modules = remote.getReferenceModules(*refParsed);
300	    EXPECT_GE(modules.size(), 1);
301
302	    // Non-existent ref
303	    auto nonExistentRef = package::Reference::parse("org.notfound.app/9.9.9/x86_64/binary");
304	    ASSERT_TRUE(nonExistentRef.has_value());
305	    auto emptyModules = remote.getReferenceModules(*nonExistentRef);
306	    EXPECT_TRUE(emptyModules.empty());
307	}
308
309	TEST_F(RepoDeepSuiteTest, RepoConfigPriorityOperations)
310	{
311	    api::types::v1::RepoConfigV2 cfg{
312	        .defaultRepo = "repo-mid",
313	        .repos = {
314	            api::types::v1::Repo{ .name = "repo-low", .priority = 5, .url = "http://low.org" },
315	            api::types::v1::Repo{ .name = "repo-mid", .priority = 20, .url = "http://mid.org" },
316	            api::types::v1::Repo{ .name = "repo-high", .priority = 50, .url = "http://high.org" },
317	            api::types::v1::Repo{ .name = "repo-mid-dup", .priority = 20, .url = "http://mid2.org" },
318	        },
319	        .version = 2,
320	    };
321
322	    EXPECT_EQ(getRepoMinPriority(cfg), 5);
323	    EXPECT_EQ(getRepoMaxPriority(cfg), 50);
324
325	    const auto &def = getDefaultRepo(cfg);
326	    EXPECT_EQ(def.name, "repo-mid");
327
328	    auto sortedRepos = getPrioritySortedRepos(cfg);
329	    ASSERT_EQ(sortedRepos.size(), 4);
330	    EXPECT_EQ(sortedRepos.front().name, "repo-high");
331	    EXPECT_EQ(sortedRepos.back().name, "repo-low");
332
333	    auto groupedRepos = getPriorityGroupedRepos(cfg);
334	    ASSERT_EQ(groupedRepos.size(), 3); // Priorities: 50, 20 (2 repos), 5
335	    EXPECT_EQ(groupedRepos[0].size(), 1);
336	    EXPECT_EQ(groupedRepos[1].size(), 2);
337	    EXPECT_EQ(groupedRepos[2].size(), 1);
338	}
339
340	TEST_F(RepoDeepSuiteTest, RepoConfigFileSaveAndLoadMultiFiles)
341	{
342	    auto cfgFile1 = tempDir.path() / "repo1.json";
343	    auto cfgFile2 = tempDir.path() / "repo2.json";
344
345	    api::types::v1::RepoConfigV2 cfg1{
346	        .defaultRepo = "base",
347	        .repos = { api::types::v1::Repo{ .name = "base", .priority = 10, .url = "http://base.org" } },
348	        .version = 2,
349	    };
350	    api::types::v1::RepoConfigV2 cfg2{
351	        .defaultRepo = "extra",
352	        .repos = { api::types::v1::Repo{ .name = "extra", .priority = 30, .url = "http://extra.org" } },
353	        .version = 2,
354	    };
355
356	    ASSERT_TRUE(saveConfig(cfg1, cfgFile1).has_value());
357	    ASSERT_TRUE(saveConfig(cfg2, cfgFile2).has_value());
358
359	    auto loaded1 = loadConfig(cfgFile1);
360	    ASSERT_TRUE(loaded1.has_value());
361	    EXPECT_EQ(loaded1->defaultRepo, "base");
362	    EXPECT_EQ(loaded1->repos.size(), 1);
363
364	    auto loadedMulti = loadConfig(std::vector<fs::path>{ cfgFile1, cfgFile2 });
365	    ASSERT_TRUE(loadedMulti.has_value());
366	    EXPECT_EQ(loadedMulti->repos.size(), 2);
367	}
368
369	TEST_F(RepoDeepSuiteTest, RepoConfigFileMalformedOrCorrupted)
370	{
371	    auto invalidJsonFile = tempDir.path() / "corrupted.json";
372	    {
373	        std::ofstream ofs(invalidJsonFile);
374	        ofs << "{ invalid json content ...";
375	    }
376
377	    auto loadRes = loadConfig(invalidJsonFile);
378	    EXPECT_FALSE(loadRes.has_value());
379
380	    auto nonExistentFile = tempDir.path() / "does_not_exist.json";
381	    auto loadRes2 = loadConfig(nonExistentFile);
382	    EXPECT_FALSE(loadRes2.has_value());
383	}
384
385	TEST_F(RepoDeepSuiteTest, RepoConfigV1ToV2Conversion)
386	{
387	    api::types::v1::RepoConfig cfgV1{
388	        .repo = {
389	            api::types::v1::RepoConfigRepoItem{
390	                .alias = std::nullopt,
391	                .name = "v1-official",
392	                .priority = 15,
393	                .url = "https://mirror.deepin.org/v1",
394	            },
395	            api::types::v1::RepoConfigRepoItem{
396	                .alias = std::nullopt,
397	                .name = "v1-community",
398	                .priority = 25,
399	                .url = "https://mirror.deepin.org/v1-community",
400	            },
401	        },
402	    };
403
404	    auto cfgV2 = convertToV2(cfgV1);
405	    EXPECT_EQ(cfgV2.version, 2);
406	    EXPECT_EQ(cfgV2.defaultRepo, "v1-community");
407	    ASSERT_EQ(cfgV2.repos.size(), 2);
408	}
409
410	TEST_F(RepoDeepSuiteTest, RepoCacheMultipleArchitectureQueries)
411	{
412	    auto cacheFile = tempDir.path() / "arch_cache.json";
413	    RepoCache cache(cacheFile);
414
415	    auto layerX64 = makeLayer("c-x64", "org.deepin.calc", "1.0.0");
416	    layerX64.info.arch = { "x86_64" };
417
418	    auto layerArm = makeLayer("c-arm", "org.deepin.calc", "1.0.0");
419	    layerArm.info.arch = { "aarch64" };
420
421	    auto layerLoong = makeLayer("c-loong", "org.deepin.calc", "1.0.0");
422	    layerLoong.info.arch = { "loongarch64" };
423
424	    ASSERT_TRUE(cache.addLayerItem(layerX64).has_value());
425	    ASSERT_TRUE(cache.addLayerItem(layerArm).has_value());
426	    ASSERT_TRUE(cache.addLayerItem(layerLoong).has_value());
427
428	    auto x64Results = cache.queryLayerItem(repoCacheQuery{
429	        .id = "org.deepin.calc",
430	        .architecture = "x86_64",
431	    });
432	    ASSERT_EQ(x64Results.size(), 1);
433	    EXPECT_EQ(x64Results.front().commit, "c-x64");
434
435	    auto armResults = cache.queryLayerItem(repoCacheQuery{
436	        .id = "org.deepin.calc",
437	        .architecture = "aarch64",
438	    });
439	    ASSERT_EQ(armResults.size(), 1);
440	    EXPECT_EQ(armResults.front().commit, "c-arm");
441
442	    auto loongResults = cache.queryLayerItem(repoCacheQuery{
443	        .id = "org.deepin.calc",
444	        .architecture = "loongarch64",
445	    });
446	    ASSERT_EQ(loongResults.size(), 1);
447	    EXPECT_EQ(loongResults.front().commit, "c-loong");
448
449	    auto riscvResults = cache.queryLayerItem(repoCacheQuery{
450	        .id = "org.deepin.calc",
451	        .architecture = "riscv64",
452	    });
453	    EXPECT_TRUE(riscvResults.empty());
454	}
455
456	TEST_F(RepoDeepSuiteTest, RepoCacheFindMatchingItemIterators)
457	{
458	    auto cacheFile = tempDir.path() / "matching_cache.json";
459	    RepoCache cache(cacheFile);
460
461	    auto item1 = makeLayer("comm-1", "org.sample.pkg1", "2.1.0");
462	    auto item2 = makeLayer("comm-2", "org.sample.pkg2", "3.0.0");
463	    ASSERT_TRUE(cache.addLayerItem(item1).has_value());
464	    ASSERT_TRUE(cache.addLayerItem(item2).has_value());
465
466	    auto itRes = cache.findMatchingItem(item2);
467	    ASSERT_TRUE(itRes.has_value());
468	    EXPECT_EQ((*itRes)->commit, "comm-2");
469
470	    auto nonExistent = makeLayer("comm-none", "org.sample.pkg_none", "1.0.0");
471	    auto itNone = cache.findMatchingItem(nonExistent);
472	    EXPECT_FALSE(itNone.has_value());
473	}
474
475	TEST_F(RepoDeepSuiteTest, RepoCacheChannelFiltering)
476	{
477	    auto cacheFile = tempDir.path() / "channel_cache.json";
478	    RepoCache cache(cacheFile);
479
480	    auto layerMain = makeLayer("c-main", "org.deepin.app", "1.0.0");
481	    layerMain.info.channel = "main";
482
483	    auto layerEdge = makeLayer("c-edge", "org.deepin.app", "1.1.0-alpha");
484	    layerEdge.info.channel = "edge";
485
486	    auto layerBeta = makeLayer("c-beta", "org.deepin.app", "1.0.1-beta");
487	    layerBeta.info.channel = "beta";
488
489	    ASSERT_TRUE(cache.addLayerItem(layerMain).has_value());
490	    ASSERT_TRUE(cache.addLayerItem(layerEdge).has_value());
491	    ASSERT_TRUE(cache.addLayerItem(layerBeta).has_value());
492
493	    auto mainItems = cache.queryLayerItem(repoCacheQuery{
494	        .id = "org.deepin.app",
495	        .channel = "main",
496	    });
497	    ASSERT_EQ(mainItems.size(), 1);
498	    EXPECT_EQ(mainItems.front().commit, "c-main");
499
500	    auto edgeItems = cache.queryLayerItem(repoCacheQuery{
501	        .id = "org.deepin.app",
502	        .channel = "edge",
503	    });
504	    ASSERT_EQ(edgeItems.size(), 1);
505	    EXPECT_EQ(edgeItems.front().commit, "c-edge");
506
507	    auto betaItems = cache.queryLayerItem(repoCacheQuery{
508	        .id = "org.deepin.app",
509	        .channel = "beta",
510	    });
511	    ASSERT_EQ(betaItems.size(), 1);
512	    EXPECT_EQ(betaItems.front().commit, "c-beta");
513	}
514
515	TEST_F(RepoDeepSuiteTest, RepoCacheQueryByKindAndModule)
516	{
517	    auto cacheFile = tempDir.path() / "kind_mod_cache.json";
518	    RepoCache cache(cacheFile);
519
520	    auto itemBin = makeLayer("c-bin", "org.deepin.suite", "1.0.0", "stable");
521	    itemBin.info.kind = "app";
522	    itemBin.info.packageInfoV2Module = "binary";
523
524	    auto itemDev = makeLayer("c-dev", "org.deepin.suite", "1.0.0", "stable");
525	    itemDev.info.kind = "app";
526	    itemDev.info.packageInfoV2Module = "develop";
527
528	    auto itemRuntime = makeLayer("c-run", "org.deepin.Runtime", "23.0.0", "stable");
529	    itemRuntime.info.kind = "runtime";
530	    itemRuntime.info.packageInfoV2Module = "runtime";
531
532	    ASSERT_TRUE(cache.addLayerItem(itemBin).has_value());
533	    ASSERT_TRUE(cache.addLayerItem(itemDev).has_value());
534	    ASSERT_TRUE(cache.addLayerItem(itemRuntime).has_value());
535
536	    auto binItems = cache.queryLayerItem(repoCacheQuery{
537	        .id = "org.deepin.suite",
538	        .module = "binary",
539	    });
540	    ASSERT_EQ(binItems.size(), 1);
541	    EXPECT_EQ(binItems.front().commit, "c-bin");
542
543	    auto devItems = cache.queryLayerItem(repoCacheQuery{
544	        .id = "org.deepin.suite",
545	        .module = "develop",
546	    });
547	    ASSERT_EQ(devItems.size(), 1);
548	    EXPECT_EQ(devItems.front().commit, "c-dev");
549
550	    auto runtimeItems = cache.queryLayerItem(repoCacheQuery{
551	        .id = "org.deepin.Runtime",
552	        .module = "runtime",
553	    });
554	    ASSERT_EQ(runtimeItems.size(), 1);
555	    EXPECT_EQ(runtimeItems.front().commit, "c-run");
556	}
557
558	} // namespace
559
560	} // namespace linglong::repo::test
561